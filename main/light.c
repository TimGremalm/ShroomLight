#include "light.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include <stdint.h>
#include <string.h>

#include "apa106.h"
#include "shroomlistener.h"

#define APA106_PIN		19
#define LED_NUM			7

static const char *TAG = "Shroom Light";
enum LIGHTSTATE state[7];
uint8_t color[7] = {0};
int breathecycle[7];
int wavecycle[7];
uint32_t previoustriggeruniqs[7][10];

trigger_t triggers[7];

void addTriggerToPreviousUnique(int shroomnr, uint32_t uniqueorigin) {
	//Shigt array over to leave room for 1 new
	for (int i=8; i>0; i--) {
		previoustriggeruniqs[shroomnr][i+1] = previoustriggeruniqs[shroomnr][i];
	}
	previoustriggeruniqs[shroomnr][0] = uniqueorigin;
}

uint32_t isUniueHandeledBefore(int shroomnr, uint32_t uniqueorigin) {
	for (int i=0; i<7; i++) {
		if (previoustriggeruniqs[shroomnr][i] == uniqueorigin) {
			//It's been handled before
			return previoustriggeruniqs[shroomnr][i];
		}
	}
	//Unique is not found, return zero
	return 0;
}

void sendTrigger(int shroomnr, char macorigin[12], int hops, int wavegen, int x, int y, int z, uint32_t uniqueorigin) {
	ESP_LOGI(TAG, "Send Trigger to %d", shroomnr);
	//If the same wave returns, ignore it
	if (isUniueHandeledBefore(shroomnr, uniqueorigin) != 0) {
		ESP_LOGI(TAG, "Wave %d handeled before, ignore", uniqueorigin);
		return;
	}
	triggers[shroomnr].arrived = xTaskGetTickCount();
	triggers[shroomnr].shroomnr = shroomnr;
	memcpy(triggers[shroomnr].macorigin, macorigin, 12);
	triggers[shroomnr].hops = hops;
	triggers[shroomnr].x = x;
	triggers[shroomnr].y = y;
	triggers[shroomnr].z = z;
	triggers[shroomnr].uniqueorigin = uniqueorigin;
	//If already waiting for a trigger, or is in a trigger, escalate the wavegen
	if (triggers[shroomnr].macorigin[0] != 0 &&
			strncmp(triggers[shroomnr].macorigin, macorigin, 12) != 0) {
		triggers[shroomnr].wavegen += 1;
	} else {
		triggers[shroomnr].wavegen = wavegen;
	}
	addTriggerToPreviousUnique(shroomnr, uniqueorigin);
}

void checkTriggers() {
	int delta;
	for (int i = 0; i < 7; i++) {
		if (triggers[i].macorigin[0] != 0) {
			delta = xTaskGetTickCount() - triggers[i].arrived;
			delta = delta * portTICK_PERIOD_MS;
			//Throw away if too many hops
			if (triggers[i].hops > 30) {
				memset(triggers[i].macorigin, 0, 12);
			}
			//After some delay, trigger the shroom, send message
			if (delta > 3000) {
				if (triggers[i].wavegen == 0 || triggers[i].wavegen == 1) {
					setShroomLightState(i, LIGHTSTATE_WaveLight);
				} else if (triggers[i].wavegen == 2) {
					setShroomLightState(i, LIGHTSTATE_WaveMedium);
				} else if (triggers[i].wavegen > 2) {
					setShroomLightState(i, LIGHTSTATE_WaveHard);
				}
				//Send shroom message, echo it forward from a new address
				sendShroomWave(i, triggers[i].macorigin, triggers[i].hops + 1, triggers[i].wavegen, triggers[i].uniqueorigin);
				//Remove trigger
				memset(triggers[i].macorigin, 0, 12);
			}
		}
	}
}

void lighttask(void *pvParameters) {
	ESP_LOGI(TAG, "Light init");
	gpio_reset_pin(APA106_PIN);
	gpio_set_direction(APA106_PIN, GPIO_MODE_DEF_OUTPUT);
	gpio_set_level(APA106_PIN, 0);
	apa106_init(APA106_PIN);
	rgbVal *pixels;
	pixels = malloc(sizeof(rgbVal) * LED_NUM);
	state[0] = LIGHTSTATE_Idle;
	state[1] = LIGHTSTATE_Idle;
	state[2] = LIGHTSTATE_Idle;
	state[3] = LIGHTSTATE_Idle;
	state[4] = LIGHTSTATE_Idle;
	state[5] = LIGHTSTATE_Idle;
	state[6] = LIGHTSTATE_Idle;

	const int breathetime = 170;
	const int wavelighttime = 120;
	const int wavehardtime = 1023;
	uint8_t volume[7];
	int wavecyclemod;
	float volumeFactor;
	for(int j = 0; j < 7; j++) {
		//Randomize start sequence
		breathecycle[j] = rand() % (300 + 1 - 0) + 0;
	}
	hsvVal hsv;
	while(1) {
		checkTriggers();
		//Go through each of the 7 shrooms
		for( int shroomid = 0; shroomid < 7; shroomid++) {
			switch(state[shroomid]) {
				case LIGHTSTATE_Off:
					//Just turn volume down in off state
					hsv.v = 0;
					pixels[shroomid] = apa106_HsvToRgb(hsv);
					break;

				case LIGHTSTATE_Solid:
					//Just turn volume down in off state
					hsv.v = 30;
					hsv.s = 255;
					if (shroomid <= 1) {
						hsv.h = 0;
					} else {
						hsv.h = 127;
					}
					pixels[shroomid] = apa106_HsvToRgb(hsv);
					break;

				case LIGHTSTATE_Idle:
					//Breathe - slow in, fast out, at random colors
					if (breathecycle[shroomid] >= 0 && breathecycle[shroomid] <= breathetime) {
						breathecycle[shroomid] += 1;
						volume[shroomid] = (uint8_t)((20.0/breathetime) * breathecycle[shroomid]);
					} else if (breathecycle[shroomid] > breathetime && breathecycle[shroomid] < breathetime*2) {
						volume[shroomid] = (uint8_t)((20.0/breathetime) * ((breathetime*2) - breathecycle[shroomid]));
						breathecycle[shroomid] += 3;
					} else if (breathecycle[shroomid] > breathetime*2 && breathecycle[shroomid] < (breathetime*2 + 50)) {
						//Extra pause after breath
						//Make pause random ???
						breathecycle[shroomid] += 3;
						volume[shroomid] = 0;
					} else {
						volume[shroomid] = 0;
						//Reset breathing
						breathecycle[shroomid] = 0;
						//Set random color in green/blue range
						color[shroomid] = rand() % (140 + 1 - 100) + 100;
					}
					hsv.v = volume[shroomid];
					hsv.h = color[shroomid];
					hsv.s = 255;
					pixels[shroomid] = apa106_HsvToRgb(hsv);
					break;

				case LIGHTSTATE_WaveLight: //Slow up fade dim random red color
				case LIGHTSTATE_WaveMedium: //Slow up fade bright white
					if (state[shroomid] == 3) {
						volumeFactor = 50.0;
					} else {
						volumeFactor = 220.0;
					}
					if (wavecycle[shroomid] >= 0 && wavecycle[shroomid] <= wavelighttime) {
						//Fade up
						volume[shroomid] = (uint8_t)((volumeFactor/wavelighttime) * wavecycle[shroomid]);
					} else if (wavecycle[shroomid] > wavelighttime && wavecycle[shroomid] < wavelighttime*5) {
						//Fade down/up  times
						wavecyclemod = wavecycle[shroomid] % wavelighttime;
						if (wavecyclemod < (wavelighttime/2)) {
							volume[shroomid] = (uint8_t)((volumeFactor/wavelighttime) * (wavelighttime - wavecyclemod));
						} else {
							volume[shroomid] = (uint8_t)((volumeFactor/wavelighttime) * wavecyclemod);
						}
					} else if (wavecycle[shroomid] > wavelighttime*8 && wavecycle[shroomid] < wavelighttime*6) {
						wavecyclemod = wavecycle[shroomid] % wavelighttime;
						volume[shroomid] = (uint8_t)((volumeFactor/wavelighttime) * (wavelighttime - wavecyclemod));
					} else {
						setShroomLightState(shroomid, LIGHTSTATE_Idle);
					}
					wavecycle[shroomid] += 1;
					hsv.v = volume[shroomid];
					hsv.h = color[shroomid];
					hsv.s = 80;
					pixels[shroomid] = apa106_HsvToRgb(hsv);
					break;

				case LIGHTSTATE_WaveHard:
					//Fast rainbow, long duration
					if (wavecycle[shroomid] >= 0 && wavecycle[shroomid] < wavehardtime) {
						//Scroll through color wheel a bunch of times
						wavecyclemod = wavecycle[shroomid] % 255;
						color[shroomid] = wavecyclemod;
					} else {
						setShroomLightState(shroomid, LIGHTSTATE_Idle);
					}
					wavecycle[shroomid] += 2;
					hsv.h = color[shroomid];
					hsv.v = 255;
					hsv.s = 255;
					pixels[shroomid] = apa106_HsvToRgb(hsv);
					break;
			}
		}
		apa106_setColors(LED_NUM, pixels);
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

void setShroomLightState(int shroomid, enum LIGHTSTATE newstate) {
	uint8_t tempcolor;
	switch (newstate) {
		case LIGHTSTATE_Idle:
			//Set random color in green/blue range
			color[shroomid] = rand() % (140 + 1 - 100) + 100;
			//Start random in cycle
			breathecycle[shroomid] = rand() % (50 + 1 - 0) + 0;
			break;
		case LIGHTSTATE_WaveLight:
		case LIGHTSTATE_WaveMedium:
		case LIGHTSTATE_WaveHard:
			//Set random color in green/blue range
			tempcolor = rand() % (140 + 1 - 100) + 100;
			//Bitsift loop around to get between 240 - 15 in red color range
			color[shroomid] = tempcolor << 1 | tempcolor >> 7;
			wavecycle[shroomid] = 0;
			break;
		case LIGHTSTATE_Off:
		case LIGHTSTATE_Solid:
			//No reset for others
			break;
	}
	state[shroomid] = newstate;
}

