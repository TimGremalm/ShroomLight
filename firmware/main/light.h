#ifndef LIGHT_H
#define LIGHT_H

#include <stdint.h>
#include "shroom_settings.h"
#include "store_mesh_idx_nvs.h"

void lighttask(void *pvParameters);

enum LIGHTSTATE {
	LIGHTSTATE_Idle,
	LIGHTSTATE_Off,
	LIGHTSTATE_Solid,
	LIGHTSTATE_WaveLight,
	LIGHTSTATE_WaveMedium,
	LIGHTSTATE_WaveHard
};

typedef struct {
	uint32_t arrived;
	uint8_t shroomnr;
	uint16_t macorigin;
	uint16_t hops;
	uint8_t wavegen;
	int16_t x;
	int16_t y;
	int16_t z;
	uint32_t uniqueorigin;
} trigger_t;

typedef struct {
	shroomsettings_t *shroomsettings;
	example_info_store_t *store;
	void (*send_wave_cb)(uint32_t, uint16_t, uint16_t, uint8_t, int16_t, int16_t, int16_t);
} light_config_t;

void sendPirTrigger();
void sendTrigger(uint8_t shroomnr, uint16_t macorigin, uint16_t hops, uint8_t wavegen, int16_t x, int16_t y, int16_t z, uint32_t uniqueorigin);
void setShroomLightState(int shroomid, enum LIGHTSTATE newstate);
enum LIGHTSTATE getShroomLightState(int shroomid);

#endif /* LIGHT_H */

