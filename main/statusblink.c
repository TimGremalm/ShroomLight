#include "statusblink.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"

static const char *TAG = "StatusBlink";
statsblink_config_t statsblink_config;

void blink(int timeon, int timeoff) {
	gpio_set_level(BLINK_GPIO, 1);
	vTaskDelay(timeon / portTICK_PERIOD_MS);
	gpio_set_level(BLINK_GPIO, 0);
	vTaskDelay(timeoff / portTICK_PERIOD_MS);
}

void setblinkstate(enum BLINKSTATE state) {
	statsblink_config.state = state;
}

void statusblinktask(void *pvParameters) {
	statsblink_config = *(statsblink_config_t *) pvParameters;

	ESP_LOGI(TAG, "Start blink %d", statsblink_config.state);
	vTaskDelay(500 / portTICK_PERIOD_MS);

	gpio_pad_select_gpio(BLINK_GPIO);
	gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
	while(1) {
		switch(statsblink_config.state) {
			case BLINKSTATE_Idle:
				blink(800, 800);
				break;
			case BLINKSTATE_Detect:
				blink(100, 100);
				break;
			case BLINKSTATE_OTA:
				blink(500, 50);
				break;
		}
	}
}

