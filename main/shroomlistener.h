#ifndef SHROOMLISTENER_H
#define SHROOMLISTENER_H

typedef struct {
	int test;
} shroomlistener_config_t;
void shroomlistenertask(void *pvParameters);
void shroom_send(char * message);
void shroom_send_info();
void sendShroomWave(int shroomnr, char macorigin[12], int hops, int wavegen);

#endif /* SHROOMLISTENER_H */

