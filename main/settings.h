#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

typedef struct {
	uint8_t gridX;
	uint8_t gridY;
	uint8_t gridZ;
} settings_t;

settings_t settings;

void LoadSettings();
void SaveSettings();

#endif /* SETTINGS_H */

