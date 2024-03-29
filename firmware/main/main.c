/* ShroomLight by Tim Gremalm
http://tim.gremalm.se/
tim@gremalm.se */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_provisioning_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Bluetooth Mesh Vendor Model Shroom Light */
#include "shroom_mesh_model.h"

/* Store mesh keys on NVS flash */
#include "ble_mesh_example_init.h"
#include "ble_mesh_example_nvs.h"
#include "store_mesh_idx_nvs.h"

/* Application specific utils for Shroom Light */
#include "shroom_settings.h"
#include "coordinates.h"

/* Application specific tasks for Shroom Light */
#include "statusblink.h"
#include "light.h"
#include "pir.h"

statsblink_config_t statsblinkconfig;
light_config_t lightconfig;

#define TAG "ShroomLight"

static uint8_t dev_uuid[16] = { 0xdd, 0xdd };

static example_info_store_t store = {
    .net_idx = ESP_BLE_MESH_KEY_UNUSED,
    .server_app_idx = ESP_BLE_MESH_KEY_UNUSED,
    .client_app_idx = ESP_BLE_MESH_KEY_UNUSED,
    .tid = 0x0,
    .addr = 0,
};

static nvs_handle_t NVS_HANDLE;
static const char * NVS_KEY = "vendor_client";

static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
};

static esp_ble_mesh_client_t vendor_client;

/* Vendor-OP-Code=3 + Maximum payload length of MESH_SHROOM_MODEL_WAVE_t=15 */
ESP_BLE_MESH_MODEL_PUB_DEFINE(vendor_client_pub, 3 + 15, ROLE_NODE);

static esp_ble_mesh_model_t vnd_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VENDOR_MODEL_ID_SHROOM_SERVER, shroom_op_server, NULL, NULL),
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VENDOR_MODEL_ID_SHROOM_CLIENT, shroom_op_client, &vendor_client_pub, &vendor_client),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};

/* Disable OOB security for SILabs Android app */
static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
#if 0
    .output_size = 4,
    .output_actions = ESP_BLE_MESH_DISPLAY_NUMBER,
    .input_actions = ESP_BLE_MESH_PUSH,
    .input_size = 4,
#else
    .output_size = 0,
    .output_actions = 0,
#endif
};

static void mesh_shroom_key_info_store(void) {
    esp_err_t err = ESP_OK;
    err = ble_mesh_nvs_store(NVS_HANDLE, NVS_KEY, &store, sizeof(store));
    if (err) {
        ESP_LOGE(TAG, "Couldn't store.");
    }
}

static void mesh_shroom_key_info_restore(void) {
    esp_err_t err = ESP_OK;
    bool exist = false;
    err = ble_mesh_nvs_restore(NVS_HANDLE, NVS_KEY, &store, sizeof(store), &exist);
    if (err != ESP_OK) {
        return;
    }
    if (exist) {
        ESP_LOGI(TAG, "Restore, net_idx 0x%04x, client app_idx 0x%04x, server app_idx 0x%04x, tid 0x%02x",
            store.net_idx, store.client_app_idx, store.server_app_idx, store.tid);
    }
}

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index) {
    ESP_LOGI(TAG, "Provision complete, store Net key.");
    ESP_LOGI(TAG, "net_idx: 0x%04x, addr: 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags: 0x%02x, iv_index: 0x%08x", flags, iv_index);
    /* Store net key index for later publish use */
    store.net_idx = net_idx;
    /* Store node address reference to identify self later */
    store.addr = addr;
    ESP_LOGI(TAG, "Node address %X", addr);
    /* mesh_shroom_key_info_store() shall not be invoked here, because if the device
     * is restarted and goes into a provisioned state, then the following events
     * will come:
     * 1st: ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT
     * 2nd: ESP_BLE_MESH_PROV_REGISTER_COMP_EVT
     * So the store.net_idx will be updated here, and if we store the mesh example
     * info here, the wrong app_idx (initialized with 0xFFFF) will be stored in nvs
     * just before restoring it.
     */
}

static void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param) {
    switch (event) {
        case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
            /* Initialize BLE Mesh provisioning capabilities and internal data information completion event */
            ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
            /* Restore proper NVS mesh info */
            mesh_shroom_key_info_restore();
            break;
        case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
            /* Enable node provisioning functionality completion event */
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
            /* Establish a BLE Mesh link event */
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
                param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
            /* Close a BLE Mesh link event */
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
                param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
            /* Provisioning done event */
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
            prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
                param->node_prov_complete.flags, param->node_prov_complete.iv_index);
            break;
        case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
            /* Provisioning reset event */
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_RESET_EVT");
            break;
        case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
            /* Set the unprovisioned device name completion event */
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d", param->node_set_unprov_dev_name_comp.err_code);
            break;
        default:
            break;
    }
}

static void ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event, esp_ble_mesh_model_cb_param_t *param) {
    esp_err_t err;
    switch (event) {
        case ESP_BLE_MESH_MODEL_OPERATION_EVT:
            /*!< User-defined models receive messages from peer devices (e.g. get, set, status, etc) event */
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OPERATION_EVT net_idx %X app_idx %X Remote address %X Destination address %X",
                            param->model_operation.ctx->net_idx,
                            param->model_operation.ctx->app_idx,
                            param->model_operation.ctx->addr,
                            param->model_operation.ctx->recv_dst
                            );
            switch (param->model_operation.opcode) {
                /* ------ server messages treatment ------ */
                case MESH_SHROOM_MODEL_OP_COORDINATE_GET:
                    ESP_LOGI(TAG, "MESH_SHROOM_MODEL_OP_COORDINATE_GET");
                    MESH_SHROOM_MODEL_COORDINATE_t coordinate;
                    coordinate.x = shroomsettings.gridX;
                    coordinate.y = shroomsettings.gridY;
                    coordinate.z = shroomsettings.gridZ;
                    err = esp_ble_mesh_server_model_send_msg(&vnd_models[0],
                                                            param->model_operation.ctx, MESH_SHROOM_MODEL_OP_COORDINATE_GET_STATUS,
                                                            sizeof(coordinate.raw), coordinate.raw);
                    if (err) {
                        ESP_LOGE(TAG, "Failed to send get status message 0x%06x", MESH_SHROOM_MODEL_OP_COORDINATE_GET_STATUS);
                    }
                    break;
                case MESH_SHROOM_MODEL_OP_COORDINATE_SET:
                    ESP_LOGI(TAG, "MESH_SHROOM_MODEL_OP_COORDINATE_SET");
                    MESH_SHROOM_MODEL_COORDINATE_t setcoord;
                    memcpy(setcoord.raw, param->model_operation.msg, param->model_operation.length);
                    ESP_LOGI(TAG, "Set coordinate to: %d %d %d", setcoord.x, setcoord.y, setcoord.z);
                    err = esp_ble_mesh_server_model_send_msg(&vnd_models[0],
                                                            param->model_operation.ctx, MESH_SHROOM_MODEL_OP_COORDINATE_SET_STATUS,
                                                            sizeof(setcoord.raw), setcoord.raw);
                    if (err) {
                        ESP_LOGE(TAG, "Failed to send message 0x%06x", MESH_SHROOM_MODEL_OP_COORDINATE_SET_STATUS);
                    }
                    shroomsettings.gridX = setcoord.x;
                    shroomsettings.gridY = setcoord.y;
                    shroomsettings.gridZ = setcoord.z;
                    SaveSettings();
                    calculateShroomCoordinates();
                    break;
                case MESH_SHROOM_MODEL_OP_LIGHTSTATE_GET:
                    ESP_LOGI(TAG, "MESH_SHROOM_MODEL_OP_LIGHTSTATE_GET");
                    MESH_SHROOM_MODEL_LIGHTSTATE_t state;
                    state.shroomid = 0;
                    state.state = (uint8_t)getShroomLightState(state.shroomid);
                    err = esp_ble_mesh_server_model_send_msg(&vnd_models[0],
                                                            param->model_operation.ctx, MESH_SHROOM_MODEL_OP_LIGHTSTATE_GET_STATUS,
                                                            sizeof(state.raw), state.raw);
                    if (err) {
                        ESP_LOGE(TAG, "Failed to send get status message 0x%06x", MESH_SHROOM_MODEL_OP_LIGHTSTATE_GET_STATUS);
                    }
                    break;
                case MESH_SHROOM_MODEL_OP_LIGHTSTATE_SET:
                    ESP_LOGI(TAG, "MESH_SHROOM_MODEL_OP_LIGHTSTATE_SET");
                    MESH_SHROOM_MODEL_LIGHTSTATE_t newstate;
                    memcpy(newstate.raw, param->model_operation.msg, param->model_operation.length);
                    ESP_LOGI(TAG, "Set new light state on shroom id: %d to: %d", newstate.shroomid, newstate.state);
                    if (newstate.shroomid < 7) {
                        setShroomLightState(newstate.shroomid, newstate.state);
                    } else {
                        /* Not a specific shroom, set all to value */
                        setShroomLightState(0, newstate.state);
                        setShroomLightState(1, newstate.state);
                        setShroomLightState(2, newstate.state);
                        setShroomLightState(3, newstate.state);
                        setShroomLightState(4, newstate.state);
                        setShroomLightState(5, newstate.state);
                        setShroomLightState(6, newstate.state);
                    }
                    err = esp_ble_mesh_server_model_send_msg(&vnd_models[0],
                                                            param->model_operation.ctx, MESH_SHROOM_MODEL_OP_LIGHTSTATE_SET_STATUS,
                                                            sizeof(newstate.raw), newstate.raw);
                    if (err) {
                        ESP_LOGE(TAG, "Failed to send message 0x%06x", MESH_SHROOM_MODEL_OP_LIGHTSTATE_SET_STATUS);
                    }
                    break;
                case MESH_SHROOM_MODEL_OP_WAVE:
                    ESP_LOGI(TAG, "MESH_SHROOM_MODEL_OP_WAVE");
                    MESH_SHROOM_MODEL_WAVE_t newwave;
                    memcpy(newwave.raw, param->model_operation.msg, param->model_operation.length);
                    ESP_LOGI(TAG, "Wave uniqueid: %d origin: %d hops: %d generation: %d x: %d y: %d z: %d",
                             newwave.uniqueid, newwave.origin, newwave.hops, newwave.generation,
                             newwave.x, newwave.y, newwave.z);
                    int smallest;
                    for (int i = 0; i < 7; i++) {
                        smallest = closestDistanceToShroomWaves(i, newwave.x, newwave.y, newwave.z);
                        if (smallest == 1) {
                            ESP_LOGI(TAG, "This shroom is a neighbor, trigger");
                            sendTrigger(i, newwave.origin, newwave.hops, newwave.generation, newwave.x, newwave.y, newwave.z, newwave.uniqueid);
                        }
                    }
                    break;
                default:
                    break;
            }
            break;
        case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
            /*!< User-defined models send messages completion event */
            if (param->model_send_comp.err_code) {
                ESP_LOGE(TAG, "Failed to send message 0x%06x", param->model_send_comp.opcode);
                break;
            }
            ESP_LOGI("", "msg 0x%06x sent to node 0x%04x", param->model_send_comp.opcode, param->model_send_comp.ctx->addr);
            break;
        case ESP_BLE_MESH_CLIENT_MODEL_RECV_PUBLISH_MSG_EVT:
            /*!< User-defined client models receive publish messages event */
            ESP_LOGI(TAG, "received publish message 0x%06x", param->client_recv_publish_msg.opcode);
            break;
        case ESP_BLE_MESH_CLIENT_MODEL_SEND_TIMEOUT_EVT:
            /*!< Timeout event for the user-defined client models that failed to receive response from peer server models */
            ESP_LOGW(TAG, "message 0x%06x timeout", param->client_send_timeout.opcode);
            break;
        case ESP_BLE_MESH_MODEL_PUBLISH_UPDATE_EVT:
            /*!< When a model is configured to publish messages periodically, this event will occur during every publish period */
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_PUBLISH_UPDATE_EVT");
            break;
        default:
            break;
    }
}
void wave_publish(uint32_t uniqueid, uint16_t origin, uint16_t hops, uint8_t generation, int16_t x, int16_t y, int16_t z) {
    ESP_LOGI(TAG, "Wave publish uniqueid %d origin %d hops %d generation %d x %d y %d z %d",
        uniqueid, origin, hops, generation, x, y ,z);
    MESH_SHROOM_MODEL_WAVE_t newwave;
    newwave.uniqueid = uniqueid;
    newwave.origin = origin;
    newwave.hops = hops;
    newwave.generation = generation;
    newwave.x = x;
    newwave.y = y;
    newwave.z = z;
    /* Publish wave to all units subscribed to wave client. */
    esp_err_t err = ESP_OK;
    err = esp_ble_mesh_model_publish(&vnd_models[1], MESH_SHROOM_MODEL_OP_WAVE, sizeof(newwave.raw), newwave.raw, ROLE_NODE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send vendor message 0x%06x error %X", MESH_SHROOM_MODEL_OP_WAVE, err);
        return;
    }

}

void wave_publishh(MESH_SHROOM_MODEL_WAVE_t newwave) {
    /* Publish wave to all units subscribed to wave client. */
    esp_err_t err = ESP_OK;
    err = esp_ble_mesh_model_publish(&vnd_models[1], MESH_SHROOM_MODEL_OP_WAVE, sizeof(newwave.raw), newwave.raw, ROLE_NODE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send vendor message 0x%06x error %X", MESH_SHROOM_MODEL_OP_WAVE, err);
        return;
    }
}

static void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                              esp_ble_mesh_cfg_server_cb_param_t *param) {
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        switch (param->ctx.recv_op) {
            case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
                ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
                ESP_LOGI(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                    param->value.state_change.appkey_add.net_idx,
                    param->value.state_change.appkey_add.app_idx);
                ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
                break;
            case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
                ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
                ESP_LOGI(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                    param->value.state_change.mod_app_bind.element_addr,
                    param->value.state_change.mod_app_bind.app_idx,
                    param->value.state_change.mod_app_bind.company_id,
                    param->value.state_change.mod_app_bind.model_id);
                if (param->value.state_change.mod_app_bind.company_id == CID_ESP &&
                    param->value.state_change.mod_app_bind.model_id == ESP_BLE_MESH_VENDOR_MODEL_ID_SHROOM_CLIENT) {
                    ESP_LOGI(TAG, "Store Shroom Client APP Key");
                    store.client_app_idx = param->value.state_change.mod_app_bind.app_idx;
                    mesh_shroom_key_info_store(); /* Store proper mesh shroom keys info */
                }
                if (param->value.state_change.mod_app_bind.company_id == CID_ESP &&
                    param->value.state_change.mod_app_bind.model_id == ESP_BLE_MESH_VENDOR_MODEL_ID_SHROOM_SERVER) {
                    ESP_LOGI(TAG, "Store Shroom Server APP Key");
                    store.server_app_idx = param->value.state_change.mod_app_bind.app_idx;
                    mesh_shroom_key_info_store(); /* Store proper mesh shroom keys info */
                }
                break;
            case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
                ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD");
                ESP_LOGI(TAG, "elem_addr 0x%04x, sub_addr 0x%04x, cid 0x%04x, mod_id 0x%04x",
                    param->value.state_change.mod_sub_add.element_addr,
                    param->value.state_change.mod_sub_add.sub_addr,
                    param->value.state_change.mod_sub_add.company_id,
                    param->value.state_change.mod_sub_add.model_id);
                break;
            default:
                break;
        }
    }
}

static esp_err_t ble_mesh_init(void) {
    esp_ble_mesh_register_prov_callback(ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(ble_mesh_config_server_cb);
    esp_ble_mesh_register_custom_model_callback(ble_mesh_custom_model_cb);

    esp_err_t err = ESP_OK;
    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mesh stack (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable mesh node (err %d)", err);
        return err;
    }

    ESP_LOGI(TAG, "BLE Mesh Node initialized");
    return err;
}

void app_main(void) {
    esp_err_t err;

    ESP_LOGI(TAG, "Shroom Light by Tim Gremalm");

    ESP_LOGI(TAG, "nvs_flash_init()");
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_LOGI(TAG, "ESP_ERR_NVS_NO_FREE_PAGES do a nvs_flash_erase()");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_LOGI(TAG, "nvs_flash_init()");
        err = nvs_flash_init();
        ESP_LOGI(TAG, "NVS reinitialized.");
    }
    ESP_ERROR_CHECK(err);

	LoadSettings();
	calculateShroomCoordinates();
    ESP_LOGI(TAG, "Loaded coordinated from settings x %d y %d z %d", shroomsettings.gridX, shroomsettings.gridY, shroomsettings.gridZ);

    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }

    /* Open nvs namespace for storing/restoring mesh shroom keys info */
    err = ble_mesh_nvs_open(&NVS_HANDLE);
    if (err) {
        return;
    }

    ble_mesh_get_dev_uuid(dev_uuid);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err) {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }

    esp_ble_mesh_set_unprovisioned_device_name("Shroom");

    /* Make settings accessable from light task */
    lightconfig.shroomsettings = &shroomsettings;
    lightconfig.store = &store;
    lightconfig.send_wave_cb = &wave_publish;
	xTaskCreate(&statusblinktask, "statusblinktask", 8192, &statsblinkconfig, 5, NULL);
	xTaskCreate(&lighttask, "lighttask", 8192, &lightconfig, 5, NULL);
	xTaskCreate(&pirtask, "pirtask", 8192, NULL, 5, NULL);
}
