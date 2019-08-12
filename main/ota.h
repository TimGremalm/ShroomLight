#include <stdint.h>
#define BUFFSIZE 1024
#define HASH_LEN 32 /* SHA-256 digest length */
void ota_start(char * url);
void print_sha256 (const uint8_t *image_hash, const char *label);

