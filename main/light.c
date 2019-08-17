#include "light.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>

#include "apa106.h"

#define APA106_PIN		19
#define LED_NUM			7

static const char *TAG = "Shroom Light";
enum LIGHTSTATE state[7];
uint8_t color[7] = {0};
int breathecycle[7];
int wavecycle[7];

void lighttask(void *pvParameters) {
	ESP_LOGI(TAG, "Light init");
	gpio_reset_pin(APA106_PIN);
	gpio_set_direction(APA106_PIN, GPIO_MODE_DEF_OUTPUT);
	gpio_set_level(APA106_PIN, 0);
	apa106_init(APA106_PIN);
	rgbVal *pixels;
	pixels = malloc(sizeof(rgbVal) * LED_NUM);
	state[0] = LIGHTSTATE_Off;
	state[1] = LIGHTSTATE_WaveLight;
	state[2] = LIGHTSTATE_Off;
	state[3] = LIGHTSTATE_Off;
	state[4] = LIGHTSTATE_Off;
	state[5] = LIGHTSTATE_Off;
	state[6] = LIGHTSTATE_Off;

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

