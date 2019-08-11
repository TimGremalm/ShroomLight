#include "light.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>

#include "apa106.h"

#define APA106_PIN		19
#define LED_NUM			7

static const char *TAG = "Shroom Light";

void lighttask(void *pvParameters) {
	ESP_LOGI(TAG, "Light init");
	gpio_reset_pin(APA106_PIN);
	gpio_set_direction(APA106_PIN, GPIO_MODE_DEF_OUTPUT);
	gpio_set_level(APA106_PIN, 0);
	apa106_init(APA106_PIN);
	rgbVal *pixels;
	pixels = malloc(sizeof(rgbVal) * LED_NUM);

	uint8_t color = 0;
	while(1) {
		color++;
		if (color > 200) {
			color = 0;
		}
		hsvVal hsv;
		hsv.h = color;
		hsv.v = 100;
		hsv.s = 255;
		for( int i = 0; i < LED_NUM; i++) {
			pixels[i] = apa106_HsvToRgb(hsv);
		}
		apa106_setColors(LED_NUM, pixels);
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}


