#ifndef SHROOM_SETTINGS_H
#define SHROOM_SETTINGS_H

#include <stdint.h>

typedef struct {
	int32_t gridX;
	int32_t gridY;
	int32_t gridZ;
} shroomsettings_t;

shroomsettings_t shroomsettings;

void LoadSettings();
void SaveSettings();

#endif /* SHROOM_SETTINGS_H */

