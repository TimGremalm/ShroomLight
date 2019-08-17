#include "shroomlistener.h"

#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/netif.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

#include "version.h"
#include "ota.h"
#include "settings.h"

static const char *TAG = "Listener";
ip_addr_t multiaddr;
struct netconn *conn;
uint8_t mac[6];
char macstring[12];

int indexOf(char * str, char toFind) {
	int i = 0;
	while(*str) {
		if (*str == toFind) {
			return i;
		}
		i++;
		str++;
	}
	// Return -1 as character not found
	return -1;
}

void shroomlistenertask(void *pvParameters) {
	shroomlistener_config_t listener_config = *(shroomlistener_config_t *) pvParameters;

	ESP_LOGI(TAG, "Open server %d", listener_config.test);

	err_t err;
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	sprintf(macstring, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	/* Create a new connection handle */
	conn = netconn_new(NETCONN_UDP);
	if(!conn) {
		ESP_LOGE(TAG, "Failed to allocate socket");
		return;
	}

	/* Bind to port with default IP address */
	err = netconn_bind(conn, IP_ADDR_ANY, 10420);
	if(err != ERR_OK) {
		ESP_LOGE(TAG, "Failed to bind socket. err=%d", err);
		return;
	}

	IP_ADDR4(&multiaddr, 239, 255, 0, 1); //IPv4 local scope multicast

	err = netconn_join_leave_group(conn, &multiaddr, &netif_default->ip_addr, NETCONN_JOIN);
	if(err != ERR_OK) {
		ESP_LOGE(TAG, "Join Multicast Group. err=%d", err);
		return;
	}

	LoadSettings();

	shroom_send_info();
	ESP_LOGI(TAG, "Listening for connections");

	while(1) {
		struct netbuf *buf;

		err = netconn_recv(conn, &buf);
		if(err != ERR_OK) {
			ESP_LOGE(TAG, "Failed to receive packet. err=%d", err);
			continue;
		}
		ESP_LOGI(TAG, "Received packet %d", buf->p->tot_len);

		//Arguments
		char argCommand[50];
		char argMac[13];
		int argLightMode;
		int32_t argX;
		int32_t argY;
		int32_t argZ;
		char argURL[100];
		int argHops;
		int argWaveGeneration;

		//Parse argments
		int sepCounter = 0;
		int sepStart = -1;
		int sepNext = 0;
		int argStart = 0;
		int argLength = 0;
		char argMessage[50];
		while (sepNext != -1) {
			sepCounter++;
			sepNext = indexOf((char *)buf->p->payload + sepStart + 1, (char)32);
			if (sepNext == -1) {
				//No more arguments
				argStart = sepStart + 1;
				argLength = buf->p->tot_len - argStart;
			} else {
				//More arguments to be excpected
				argStart = sepStart + 1;
				argLength = sepNext;
				sepStart += sepNext + 1;
			}
			memset(argMessage, 0, sizeof(argMessage));
			memcpy(argMessage, (char *)buf->p->payload+argStart, argLength);
			//ESP_LOGI(TAG, "Arg %d: |%s|", sepCounter, argMessage);

			//Analyze argments
			if (sepCounter == 1) {
				memset(argCommand, 0, sizeof(argCommand));
				memcpy(argCommand, argMessage, argLength);
			}
			if (sepCounter == 2) {
				if (strncmp(argCommand, "OTA", 3) == 0) {
					memset(argURL, 0, sizeof(argURL));
					memcpy(argURL, argMessage, argLength);
				}
				if (strncmp(argCommand, "SOTA", 4) == 0 ||
					strncmp(argCommand, "LIGHTMODE", 9) == 0 ||
					strncmp(argCommand, "TRIGGER", 7) == 0 ||
					strncmp(argCommand, "SETGRID", 7) == 0 ||
					strncmp(argCommand, "SHROOM", 6) == 0) {
					memset(argMac, 0, sizeof(argMac));
					memcpy(argMac, argMessage, argLength);
				}
			}
			if (sepCounter == 3) {
				if (strncmp(argCommand, "SOTA", 4) == 0) {
					memset(argURL, 0, sizeof(argURL));
					memcpy(argURL, argMessage, argLength);
				}
				if (strncmp(argCommand, "LIGHTMODE", 9) == 0) {
					argLightMode = atoi(argMessage);
				}
				if (strncmp(argCommand, "TRIGGER", 7) == 0 ||
					strncmp(argCommand, "SETGRID", 7) == 0) {
					argX = atoi(argMessage);
				}
				if (strncmp(argCommand, "SHROOM", 6) == 0) {
					argHops = atoi(argMessage);
				}
			}
			if (sepCounter == 4) {
				if (strncmp(argCommand, "TRIGGER", 7) == 0 ||
					strncmp(argCommand, "SETGRID", 7) == 0) {
					argY = atoi(argMessage);
				}
				if (strncmp(argCommand, "SHROOM", 6) == 0) {
					argWaveGeneration = atoi(argMessage);
				}
			}
			if (sepCounter == 5) {
				if (strncmp(argCommand, "TRIGGER", 7) == 0 ||
					strncmp(argCommand, "SETGRID", 7) == 0) {
					argZ = atoi(argMessage);
				}
				if (strncmp(argCommand, "SHROOM", 6) == 0) {
					argX = atoi(argMessage);
				}
			}
			if (sepCounter == 7) {
				if (strncmp(argCommand, "SHROOM", 6) == 0) {
					argY = atoi(argMessage);
				}
			}
			if (sepCounter == 6) {
				if (strncmp(argCommand, "SHROOM", 6) == 0) {
					argZ = atoi(argMessage);
				}
			}
		}
		//Act on arguments
		if (strncmp(argCommand, "restart", 7) == 0) {
			ESP_LOGI(TAG, "Restart");
			esp_restart();
		}
		if (strncmp(argCommand, "information", 11) == 0) {
			ESP_LOGI(TAG, "Information");
			shroom_send_info();
		}
		if (strncmp(argCommand, "OTA", 3) == 0) {
			if (sepCounter != 2) {
				ESP_LOGE(TAG, "OTA takes 1 argument, got %d", sepCounter-1);
			} else {
				ESP_LOGI(TAG, "OTA All URL: |%s|", argURL);
				ota_start(argURL);
			}
		}
		if (strncmp(argCommand, "SOTA", 4) == 0) {
			if (sepCounter != 3) {
				ESP_LOGE(TAG, "SOTA takes 2 arguments, got %d", sepCounter-1);
			} else {
				if (strncmp(argMac, macstring, 12) == 0) {
					ESP_LOGI(TAG, "Specific OTA for: %s URL: %s", argMac, argURL);
					ota_start(argURL);
				} else {
					ESP_LOGI(TAG, "Specific MAC not me");
				}
			}
		}
		if (strncmp(argCommand, "LIGHTMODE", 9) == 0) {
			if (sepCounter != 3) {
				ESP_LOGE(TAG, "LIGHTMODE takes 2 arguments, got %d", sepCounter-1);
			} else {
				if (strncmp(argMac, macstring, 12) == 0) {
					ESP_LOGI(TAG, "Light Mode for: %s Mode: %d", argMac, argLightMode);
				} else {
					ESP_LOGI(TAG, "Specific MAC not me");
				}
			}
		}
		if (strncmp(argCommand, "TRIGGER", 7) == 0) {
			if (sepCounter != 5) {
				ESP_LOGE(TAG, "TRIGGER takes 4 arguments, got %d", sepCounter-1);
			} else {
				if (strncmp(argMac, macstring, 12) == 0) {
					ESP_LOGI(TAG, "Trigger %s X: %d Y: %d Z: %d", argMac, argX, argY, argZ);
				} else {
					ESP_LOGI(TAG, "Specific MAC not me");
				}
			}
		}
		if (strncmp(argCommand, "SHROOM", 6) == 0) {
			if (sepCounter != 7) {
				ESP_LOGE(TAG, "SETGRID takes 6 arguments, got %d", sepCounter-1);
			} else {
				ESP_LOGI(TAG, "Wave orig %s, %d hops, wave generation %d, %d %d %d", argMac, argHops, argWaveGeneration, argX, argY, argZ);
			}
		}
		if (strncmp(argCommand, "SETGRID", 7) == 0) {
			if (sepCounter != 5) {
				ESP_LOGE(TAG, "SETGRID takes 4 arguments, got %d", sepCounter-1);
			} else {
				if (strncmp(argMac, macstring, 12) == 0) {
					ESP_LOGI(TAG, "Set grid on %s to X: %d Y: %d Z: %d", argMac, argX, argY, argZ);
					settings.gridX = argX;
					settings.gridY = argY;
					settings.gridZ = argZ;
					SaveSettings();
				} else {
					ESP_LOGI(TAG, "Specific MAC not me");
				}
			}
		}
		netbuf_delete(buf);
	}
}

void shroom_send_info() {
	char sbuf[100] = {0};
	//Report MAC address, version and physical grid address
	sprintf(sbuf, "Shroom %02x%02x%02x%02x%02x%02x Version %d Grid %d %d %d", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], version, settings.gridX, settings.gridY, settings.gridZ);
	shroom_send(sbuf);
}

void shroom_send(char * message) {
	err_t err;
	struct netbuf *sendbuf = netbuf_new();
	if (sendbuf == NULL) {
		ESP_LOGE(TAG, "send netbuf alloc failed");
		return;
	}
	netbuf_alloc(sendbuf, strlen(message));
	memcpy(sendbuf->p->payload, (void*)message, strlen(message));
	err = netconn_sendto(conn, sendbuf, &multiaddr, 10420);
	netbuf_delete(sendbuf);
	if(err != ERR_OK) {
		ESP_LOGE(TAG, "Failed to send packet. err=%d", err);
		return;
	}
}

