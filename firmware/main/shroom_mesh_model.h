#ifndef _SHROOM_MESH_MODEL_H_
#define _SHROOM_MESH_MODEL_H_

#define CID_ESP 0x02E5

#define ESP_BLE_MESH_VND_MODEL_ID_CLIENT 0x0000
#define ESP_BLE_MESH_VND_MODEL_ID_SERVER 0x0001

/* OP Code and Vendor ID */
#define MESH_SHROOM_MODEL_OP_COORDINATE_GET ESP_BLE_MESH_MODEL_OP_3(0x00, CID_ESP)
#define MESH_SHROOM_MODEL_OP_COORDINATE_SET ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)
#define MESH_SHROOM_MODEL_OP_COORDINATE_GET_STATUS ESP_BLE_MESH_MODEL_OP_3(0x02, CID_ESP)
#define MESH_SHROOM_MODEL_OP_COORDINATE_SET_STATUS ESP_BLE_MESH_MODEL_OP_3(0x03, CID_ESP)

typedef union {
        struct {
        int16_t x;
        int16_t y;
        int16_t z;
    } __attribute__((packed, aligned(1)));
    uint8_t raw[6];
} MESH_SHROOM_MODEL_COORDINATE_t;

/* Define the vendor light model operation */
static esp_ble_mesh_model_op_t shroom_op_server[] = {
    ESP_BLE_MESH_MODEL_OP(MESH_SHROOM_MODEL_OP_COORDINATE_GET, 0),
    ESP_BLE_MESH_MODEL_OP(MESH_SHROOM_MODEL_OP_COORDINATE_SET, 6),
    ESP_BLE_MESH_MODEL_OP_END,
};

#endif /* _SHROOM_MESH_MODEL_H_ */
