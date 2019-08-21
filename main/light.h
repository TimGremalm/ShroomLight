#ifndef LIGHT_H
#define LIGHT_H

#include <stdint.h>

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
	int shroomnr;
	char macorigin[12];
	int hops;
	int wavegen;
	int x;
	int y;
	int z;
	uint32_t uniqueorigin;
} trigger_t;

void sendTrigger(int shroomnr, char macorigin[12], int hops, int wavegen, int x, int y, int z, uint32_t uniqueorigin);
void setShroomLightState(int shroomid, enum LIGHTSTATE newstate);

#endif /* LIGHT_H */

