#include "ota.h"

#include <stdint.h>
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_log.h"

#include "statusblink.h"

static const char *TAG = "Shroom OTA";

/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

static void http_cleanup(esp_http_client_handle_t client) {
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
}

void print_sha256 (const uint8_t *image_hash, const char *label) {
	char hash_print[HASH_LEN * 2 + 1];
	hash_print[HASH_LEN * 2] = 0;
	for (int i = 0; i < HASH_LEN; ++i) {
		sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
	}
	ESP_LOGI(TAG, "%s: %s", label, hash_print);
}

void ota_start(char * url) {
	esp_err_t err;
	/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
	esp_ota_handle_t update_handle = 0 ;
	const esp_partition_t *update_partition = NULL;

	ESP_LOGI(TAG, "Start OTA");
	setblinkstate(BLINKSTATE_OTA);

	const esp_partition_t *configured = esp_ota_get_boot_partition();
	const esp_partition_t *running = esp_ota_get_running_partition();

	if (configured != running) {
		ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
				 configured->address, running->address);
		ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
	}
	ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
			 running->type, running->subtype, running->address);

	ESP_LOGI(TAG, "Start to Connect to Server....");

	esp_http_client_config_t config = {
		.url = url, // "http://192.168.43.2:8000/shroomlight.bin"
		.cert_pem = (char *)server_cert_pem_start,
	};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		ESP_LOGE(TAG, "Failed to initialise HTTP connection");
		setblinkstate(BLINKSTATE_OTA_FAILED);
		return;
	}
	err = esp_http_client_open(client, 0);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		esp_http_client_cleanup(client);
		setblinkstate(BLINKSTATE_OTA_FAILED);
		return;
	}
	esp_http_client_fetch_headers(client);

	update_partition = esp_ota_get_next_update_partition(NULL);
	ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
			 update_partition->subtype, update_partition->address);
	assert(update_partition != NULL);

	err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
		http_cleanup(client);
		setblinkstate(BLINKSTATE_OTA_FAILED);
		return;
	}
	ESP_LOGI(TAG, "esp_ota_begin succeeded");

	int binary_file_length = 0;
	/*deal with all receive packet*/
	while (1) {
		int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
		if (data_read < 0) {
			ESP_LOGE(TAG, "Error: SSL data read error");
			http_cleanup(client);
			setblinkstate(BLINKSTATE_OTA_FAILED);
			return;
		} else if (data_read > 0) {
			err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
			if (err != ESP_OK) {
				http_cleanup(client);
				setblinkstate(BLINKSTATE_OTA_FAILED);
				return;
			}
			binary_file_length += data_read;
			ESP_LOGD(TAG, "Written image length %d", binary_file_length);
		} else if (data_read == 0) {
			ESP_LOGI(TAG, "Connection closed,all data received");
			break;
		}
	}
	ESP_LOGI(TAG, "Total Write binary data length : %d", binary_file_length);

	if (esp_ota_end(update_handle) != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_end failed!");
		http_cleanup(client);
		setblinkstate(BLINKSTATE_OTA_FAILED);
		return;
	}

	if (esp_partition_check_identity(esp_ota_get_running_partition(), update_partition) == true) {
		ESP_LOGI(TAG, "The current running firmware is same as the firmware just downloaded");
		setblinkstate(BLINKSTATE_OTA_FAILED);
		return;
	}

	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
		http_cleanup(client);
		setblinkstate(BLINKSTATE_OTA_FAILED);
		return;
	}
	ESP_LOGI(TAG, "Prepare to restart system!");
	esp_restart();
	setblinkstate(BLINKSTATE_Idle);
	return ;
}

