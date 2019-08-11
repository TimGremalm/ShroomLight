#include "pir.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>

#include "statusblink.h"

#define PIR_PIN			27

static const char *TAG = "Shroom Pir";

void pirtask(void *pvParameters) {
	ESP_LOGI(TAG, "PIR init");
	gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(PIR_PIN, GPIO_PULLDOWN_ONLY);

	int previousPirState = 0;
	int pirState = 0;
	while(1) {
		pirState = gpio_get_level(PIR_PIN);
		if (previousPirState != pirState) {
			if (pirState == 1) {
				ESP_LOGI(TAG, "Pir Sens On");
				setblinkstate(BLINKSTATE_Detect);
			} else {
				ESP_LOGI(TAG, "Pir Sens Off");
				setblinkstate(BLINKSTATE_Idle);
			}
			previousPirState = pirState;
		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}


