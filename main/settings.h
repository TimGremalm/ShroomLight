#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

typedef struct {
	int32_t gridX;
	int32_t gridY;
	int32_t gridZ;
} settings_t;

settings_t settings;

void LoadSettings();
void SaveSettings();

#endif /* SETTINGS_H */

