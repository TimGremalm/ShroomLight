/* FU */
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

#include "board.h"
#include "ble_mesh_example_init.h"

#define TAG "ShroomLight"

#define CID_ESP 0x02E5

extern struct _led_state led_state[3];

static uint8_t dev_uuid[16] = { 0xdd, 0xdd };

#define ESP_BLE_MESH_VND_MODEL_ID_CLIENT    0x0000
#define ESP_BLE_MESH_VND_MODEL_ID_SERVER    0x0001

static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_0, 2 + 3, ROLE_NODE);
static esp_ble_mesh_gen_onoff_srv_t onoff_server_0 = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
};

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_0, &onoff_server_0),
};

/* OP Code and Vendor ID */
#define MESH_SHROOM_MODEL_OP_COORDINATE_GET ESP_BLE_MESH_MODEL_OP_3(0x00, CID_ESP)
#define MESH_SHROOM_MODEL_OP_COORDINATE_SET ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)
#define MESH_SHROOM_MODEL_OP_COORDINATE_GET_STATUS ESP_BLE_MESH_MODEL_OP_3(0x02, CID_ESP)
#define MESH_SHROOM_MODEL_OP_COORDINATE_SET_STATUS ESP_BLE_MESH_MODEL_OP_3(0x03, CID_ESP)

/* Define the vendor light model operation */
static esp_ble_mesh_model_op_t shroom_op_server[] = {
    ESP_BLE_MESH_MODEL_OP(MESH_SHROOM_MODEL_OP_COORDINATE_GET, 0),
    ESP_BLE_MESH_MODEL_OP(MESH_SHROOM_MODEL_OP_COORDINATE_SET, 6),
    ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_t vnd_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VND_MODEL_ID_SERVER,
                              shroom_op_server, NULL, NULL),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
};

typedef union {
	struct {
		int16_t x;
        int16_t y;
        int16_t z;
	} __attribute__((packed, aligned(1)));
	uint8_t raw[6];
} MESH_SHROOM_MODEL_COORDINATE_t;

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

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index) {
    ESP_LOGI(TAG, "net_idx: 0x%04x, addr: 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags: 0x%02x, iv_index: 0x%08x", flags, iv_index);
    board_led_operation(LED_G, LED_OFF);
}

static void example_change_led_state(esp_ble_mesh_model_t *model,
                                     esp_ble_mesh_msg_ctx_t *ctx, uint8_t onoff) {
    uint16_t primary_addr = esp_ble_mesh_get_primary_element_address();
    uint8_t elem_count = esp_ble_mesh_get_element_count();
    struct _led_state *led = NULL;
    uint8_t i;

    if (ESP_BLE_MESH_ADDR_IS_UNICAST(ctx->recv_dst)) {
        for (i = 0; i < elem_count; i++) {
            if (ctx->recv_dst == (primary_addr + i)) {
                led = &led_state[i];
                board_led_operation(led->pin, onoff);
            }
        }
    } else if (ESP_BLE_MESH_ADDR_IS_GROUP(ctx->recv_dst)) {
        if (esp_ble_mesh_is_model_subscribed_to_group(model, ctx->recv_dst)) {
            led = &led_state[model->element->element_addr - primary_addr];
            board_led_operation(led->pin, onoff);
        }
    } else if (ctx->recv_dst == 0xFFFF) {
        led = &led_state[model->element->element_addr - primary_addr];
        board_led_operation(led->pin, onoff);
    }
}

static void example_handle_gen_onoff_msg(esp_ble_mesh_model_t *model,
                                         esp_ble_mesh_msg_ctx_t *ctx,
                                         esp_ble_mesh_server_recv_gen_onoff_set_t *set) {
    esp_ble_mesh_gen_onoff_srv_t *srv = model->user_data;

    switch (ctx->recv_op) {
        case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
            esp_ble_mesh_server_model_send_msg(model, ctx,
                ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, sizeof(srv->state.onoff), &srv->state.onoff);
            break;
        case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:
            if (set->op_en == false) {
                srv->state.onoff = set->onoff;
            } else {
                /* TODO: Delay and state transition */
                srv->state.onoff = set->onoff;
            }
            if (ctx->recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
                esp_ble_mesh_server_model_send_msg(model, ctx,
                    ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, sizeof(srv->state.onoff), &srv->state.onoff);
            }
            esp_ble_mesh_model_publish(model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                sizeof(srv->state.onoff), &srv->state.onoff, ROLE_NODE);
            example_change_led_state(model, ctx, srv->state.onoff);
            break;
        default:
            break;
    }
}

static void example_ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param) {
    switch (event) {
        case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
            break;
        case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
                param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
                param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
            prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
                param->node_prov_complete.flags, param->node_prov_complete.iv_index);
            break;
        case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_RESET_EVT");
            break;
        case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d", param->node_set_unprov_dev_name_comp.err_code);
            break;
        default:
            break;
    }
}

static void example_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                               esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t *srv;
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04x, src 0x%04x, dst 0x%04x",
        event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    switch (event) {
        case ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT");
            if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
                param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
                ESP_LOGI(TAG, "onoff 0x%02x", param->value.state_change.onoff_set.onoff);
                example_change_led_state(param->model, &param->ctx, param->value.state_change.onoff_set.onoff);
            }
            break;
        case ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT");
            if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET) {
                srv = param->model->user_data;
                ESP_LOGI(TAG, "onoff 0x%02x", srv->state.onoff);
                example_handle_gen_onoff_msg(param->model, &param->ctx, NULL);
            }
            break;
        case ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT");
            if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
                param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
                ESP_LOGI(TAG, "onoff 0x%02x, tid 0x%02x", param->value.set.onoff.onoff, param->value.set.onoff.tid);
                if (param->value.set.onoff.op_en) {
                    ESP_LOGI(TAG, "trans_time 0x%02x, delay 0x%02x",
                        param->value.set.onoff.trans_time, param->value.set.onoff.delay);
                }
                example_handle_gen_onoff_msg(param->model, &param->ctx, &param->value.set.onoff);
            }
            break;
        default:
            ESP_LOGE(TAG, "Unknown Generic Server event 0x%02x", event);
            break;
    }
}

static void example_ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event,
                                             esp_ble_mesh_model_cb_param_t *param) {
    uint16_t d_addr;
    esp_err_t err;

    switch (event) {
        case ESP_BLE_MESH_MODEL_OPERATION_EVT:
            /*!< User-defined models receive messages from peer devices (e.g. get, set, status, etc) event */
            d_addr = param->model_operation.ctx->addr;
            switch (param->model_operation.opcode) {
                /* ------ server messages treatment ------ */
                case MESH_SHROOM_MODEL_OP_COORDINATE_GET:
                    ESP_LOGI(TAG, "MESH_SHROOM_MODEL_OP_COORDINATE_GET");
                    MESH_SHROOM_MODEL_COORDINATE_t coordinate;
                    coordinate.x = 1;
                    coordinate.y = 2;
                    coordinate.z = 3;
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
            //example_ble_mesh_send_vendor_message(ESP_BLE_MESH_VND_MODEL_OP_SET,0,d_addr);
            break;
        case ESP_BLE_MESH_MODEL_PUBLISH_UPDATE_EVT:
            /*!< When a model is configured to publish messages periodically, this event will occur during every publish period */
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_PUBLISH_UPDATE_EVT");
            //publish_temp();
            break;
        default:
            break;
    }
}

static void example_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
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
    esp_err_t err = ESP_OK;

    esp_ble_mesh_register_prov_callback(example_ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(example_ble_mesh_config_server_cb);
    esp_ble_mesh_register_generic_server_callback(example_ble_mesh_generic_server_cb);
    esp_ble_mesh_register_custom_model_callback(example_ble_mesh_custom_model_cb);

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

    board_led_operation(LED_G, LED_ON);

    return err;
}

void testtask(void *pvParameters) {
	ESP_LOGI(TAG, "testtask init");
    while(1) {
    	ESP_LOGI(TAG, "testtask loop");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    esp_err_t err;

    ESP_LOGI(TAG, "Initializing Shroom Light...");

    board_init();

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }

    ble_mesh_get_dev_uuid(dev_uuid);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err) {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }

    esp_ble_mesh_set_unprovisioned_device_name("Shroom");

    xTaskCreate(&testtask, "testtask", 8192, NULL, 5, NULL);
}
