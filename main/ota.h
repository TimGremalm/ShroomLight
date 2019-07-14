#include <stdint.h>
#define BUFFSIZE 1024
#define HASH_LEN 32 /* SHA-256 digest length */
#define EXAMPLE_SERVER_URL CONFIG_FIRMWARE_UPG_URL
void ota_start();
void print_sha256 (const uint8_t *image_hash, const char *label);

