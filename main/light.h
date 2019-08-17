#ifndef LIGHT_H
#define LIGHT_H

void lighttask(void *pvParameters);

enum LIGHTSTATE {
	LIGHTSTATE_Idle,
	LIGHTSTATE_Off,
	LIGHTSTATE_Solid,
	LIGHTSTATE_WaveLight,
	LIGHTSTATE_WaveMedium,
	LIGHTSTATE_WaveHard
};

void setShroomLightState(int shroomid, enum LIGHTSTATE newstate);

#endif /* LIGHT_H */

