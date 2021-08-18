#ifndef _SHROOM_MESH_MODEL_H_
#define _SHROOM_MESH_MODEL_H_

#include "esp_ble_mesh_defs.h"

#define CID_ESP 0x02E5

#define ESP_BLE_MESH_VND_MODEL_ID_CLIENT 0x0000
#define ESP_BLE_MESH_VND_MODEL_ID_SERVER 0x0001

/* OP Code and Vendor ID */
/* Coordinate - x, y, z position of the center shroom */
#define MESH_SHROOM_MODEL_OP_COORDINATE_GET ESP_BLE_MESH_MODEL_OP_3(0x00, CID_ESP)
#define MESH_SHROOM_MODEL_OP_COORDINATE_SET ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)
#define MESH_SHROOM_MODEL_OP_COORDINATE_GET_STATUS ESP_BLE_MESH_MODEL_OP_3(0x02, CID_ESP)
#define MESH_SHROOM_MODEL_OP_COORDINATE_SET_STATUS ESP_BLE_MESH_MODEL_OP_3(0x03, CID_ESP)

/* LightState - will it idle, be in a wave, or solis on etc.*/
#define MESH_SHROOM_MODEL_OP_LIGHTSTATE_GET ESP_BLE_MESH_MODEL_OP_3(0x04, CID_ESP)
#define MESH_SHROOM_MODEL_OP_LIGHTSTATE_SET ESP_BLE_MESH_MODEL_OP_3(0x05, CID_ESP)
#define MESH_SHROOM_MODEL_OP_LIGHTSTATE_GET_STATUS ESP_BLE_MESH_MODEL_OP_3(0x06, CID_ESP)
#define MESH_SHROOM_MODEL_OP_LIGHTSTATE_SET_STATUS ESP_BLE_MESH_MODEL_OP_3(0x07, CID_ESP)

/* Wave - Wave origin from coordinates */
#define MESH_SHROOM_MODEL_OP_WAVE ESP_BLE_MESH_MODEL_OP_3(0x08, CID_ESP)

/* Define the shroom server model operation */
static esp_ble_mesh_model_op_t shroom_op_server[] = {
    ESP_BLE_MESH_MODEL_OP(MESH_SHROOM_MODEL_OP_COORDINATE_GET, 0),
    ESP_BLE_MESH_MODEL_OP(MESH_SHROOM_MODEL_OP_COORDINATE_SET, 6),
    ESP_BLE_MESH_MODEL_OP(MESH_SHROOM_MODEL_OP_LIGHTSTATE_GET, 0),
    ESP_BLE_MESH_MODEL_OP(MESH_SHROOM_MODEL_OP_LIGHTSTATE_SET, 2),
    ESP_BLE_MESH_MODEL_OP(MESH_SHROOM_MODEL_OP_WAVE, 15),
    ESP_BLE_MESH_MODEL_OP_END,
};

/* Define the shroom client model operation */
static esp_ble_mesh_model_op_t shroom_op_client[] = {
    ESP_BLE_MESH_MODEL_OP(MESH_SHROOM_MODEL_OP_WAVE, 15),
    ESP_BLE_MESH_MODEL_OP_END,
};

/* Structs used by the shroom operations */
typedef union {
    struct {
        int16_t x;
        int16_t y;
        int16_t z;
    } __attribute__((packed, aligned(1)));
    uint8_t raw[6];
} MESH_SHROOM_MODEL_COORDINATE_t;

typedef union {
    struct {
        uint8_t shroomid;
        uint8_t state;
    } __attribute__((packed, aligned(1)));
    uint8_t raw[2];
} MESH_SHROOM_MODEL_LIGHTSTATE_t;

typedef union {
    struct {
        uint32_t id;
        uint16_t origin;
        uint16_t hops;
        uint8_t generation;
        int16_t x;
        int16_t y;
        int16_t z;
    } __attribute__((packed, aligned(1)));
    uint8_t raw[15];
} MESH_SHROOM_MODEL_WAVE_t;

#endif /* _SHROOM_MESH_MODEL_H_ */
