#ifndef SHROOMLISTENER_H
#define SHROOMLISTENER_H

#include <stdint.h>

typedef struct {
	int test;
} shroomlistener_config_t;
void shroomlistenertask(void *pvParameters);
void shroom_send(char * message);
void shroom_send_info();
void sendShroomWave(int shroomnr, char macorigin[12], int hops, int wavegen, uint32_t uniqueorigin);

#endif /* SHROOMLISTENER_H */

