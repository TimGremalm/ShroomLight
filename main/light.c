#include "light.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "Shroom Light";

void lighttask(void *pvParameters) {
	ESP_LOGI(TAG, "Light init");

	while(1) {
		ESP_LOGI(TAG, "Light loop");
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}


