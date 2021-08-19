#ifndef STORE_MESH_IDX_NVS_H
#define STORE_MESH_IDX_NVS_H

#include <stdint.h>

typedef struct {
    uint16_t net_idx;         /* NetKey Index */
    uint16_t server_app_idx;  /* AppKey Index */
    uint16_t client_app_idx;  /* AppKey Index */
    uint8_t  tid;             /* Message TID */
    uint16_t addr;            /* Node Address */
} __attribute__((packed)) example_info_store_t;

#endif /* STORE_MESH_IDX_NVS_H */

