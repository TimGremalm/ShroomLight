#include "shroom_settings.h"

#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "Settings";

void LoadSettings() {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	nvs_handle load_handle;
	err = nvs_open("storage", NVS_READWRITE, &load_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error %s opening NVS handle", esp_err_to_name(err));
	} else {
		int32_t temp = 0;
		err = nvs_get_i32(load_handle, "gridx", &temp);
		switch (err) {
			case ESP_OK:
				shroomsettings.gridX = temp;
				break;
			case ESP_ERR_NVS_NOT_FOUND:
				ESP_LOGI(TAG, "The gridX is not initialized yet %d", err);
				break;
			default:
				ESP_LOGE(TAG, "Error %s reading gridX", esp_err_to_name(err));
		}
		err = nvs_get_i32(load_handle, "gridy", &temp);
		switch (err) {
			case ESP_OK:
				shroomsettings.gridY = temp;
				break;
			case ESP_ERR_NVS_NOT_FOUND:
				ESP_LOGI(TAG, "The gridY is not initialized yet %d", err);
				break;
			default:
				ESP_LOGE(TAG, "Error %s reading gridY", esp_err_to_name(err));
		}
		err = nvs_get_i32(load_handle, "gridz", &temp);
		switch (err) {
			case ESP_OK:
				shroomsettings.gridZ = temp;
				break;
			case ESP_ERR_NVS_NOT_FOUND:
				ESP_LOGI(TAG, "The gridZ is not initialized yet %d", err);
				break;
			default:
				ESP_LOGE(TAG, "Error %s reading gridZ", esp_err_to_name(err));
		}
	}

	nvs_close(load_handle);
}

void SaveSettings() {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	nvs_handle save_handle;
	err = nvs_open("storage", NVS_READWRITE, &save_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error %s opening NVS handle", esp_err_to_name(err));
	} else {
		int32_t temp = 0;
		temp = shroomsettings.gridX;
		err = nvs_set_i32(save_handle, "gridx", temp);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "Error %s saving gridX", esp_err_to_name(err));
		}
		temp = shroomsettings.gridY;
		err = nvs_set_i32(save_handle, "gridy", temp);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "Error %s saving gridY", esp_err_to_name(err));
		}
		temp = shroomsettings.gridZ;
		err = nvs_set_i32(save_handle, "gridz", temp);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "Error %s saving gridZ", esp_err_to_name(err));
		}

		err = nvs_commit(save_handle);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "Error %s committing", esp_err_to_name(err));
		}
	}

	nvs_close(save_handle);
}

