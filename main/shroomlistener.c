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

#include "ota.h"

static const char *TAG = "Shroom Listener";
ip_addr_t multiaddr;
struct netconn *conn;
uint8_t mac[6];
char macstring[12];
uint version = 40;

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

		if (strncmp(buf->p->payload, "restart", 7) == 0) {
			ESP_LOGI(TAG, "Restart");
			esp_restart();
		}
		if (strncmp(buf->p->payload, "information", 11) == 0) {
			ESP_LOGI(TAG, "Information");
			shroom_send_info();
		}
		if (strncmp(buf->p->payload, "OTA", 3) == 0) {
			int FindSep;
			FindSep = indexOf(buf->p->payload, (char)32);
			if (FindSep < 1) {
				//Didn't find second argment
				netbuf_delete(buf);
				continue;
			}
			char url[buf->p->tot_len - FindSep - 1];
			int urlStart = FindSep + 1;
			int urlLength = buf->p->tot_len - FindSep - 1;
			ESP_LOGI(TAG, "URL Start: %d Len: %d", urlStart, urlLength);
			memcpy(url, (char *)buf->p->payload + urlStart, urlLength);
			ESP_LOGI(TAG, "OTA All URL: |%s|", url);
			ota_start(url);
		}
		if (strncmp(buf->p->payload, "SOTA", 4) == 0) {
			int sepMAC;
			int sepURL;
			sepMAC = indexOf((char *)buf->p->payload, (char)32);
			if (sepMAC < 1) {
				netbuf_delete(buf);
				continue;
			}
			int macStart = sepMAC + 1;
			sepURL = indexOf((char *)buf->p->payload+macStart, (char)32);
			int urlStart = macStart + sepURL + 1;
			int macLength = urlStart - macStart - 1;
			int urlLength = buf->p->tot_len - urlStart;
			//ESP_LOGI(TAG, "Separator MAC: %d", sepMAC);
			//ESP_LOGI(TAG, "Separator URL: %d", sepURL);
			//ESP_LOGI(TAG, "MAC Start: %d Len: %d", macStart, macLength);
			//ESP_LOGI(TAG, "URL Start: %d Len: %d", urlStart, urlLength);
			if (sepURL < 1) {
				netbuf_delete(buf);
				continue;
			}
			char messageMAC[macLength];
			memset(&messageMAC, 0, sizeof(messageMAC));
			char messageURL[46];
			memset(&messageURL, 0, sizeof(messageURL));
			memcpy(messageMAC, (char *)buf->p->payload+macStart, macLength);
			memcpy(messageURL, (char *)buf->p->payload+urlStart, urlLength);
			//ESP_LOGI(TAG, "MAC: |%s|", messageMAC);
			//ESP_LOGI(TAG, "URL: |%s|", messageURL);
/*
SOTA b4e62dda5709 http://192.168.43.2:8000/build/shroomlight.bin
*/
			if (strncmp(messageMAC, macstring, 12) == 0) {
				ESP_LOGI(TAG, "Specific OTA for: %s URL: %s", messageMAC, messageURL);
				ota_start(messageURL);
			} else {
				ESP_LOGI(TAG, "Specific MAC not me");
			}
		}
		netbuf_delete(buf);
	}
}

void shroom_send_info() {
	char sbuf[100] = {0};
	//Report MAC address, version and physical grid address
	sprintf(sbuf, "Shroom %02x%02x%02x%02x%02x%02x Version %d Grid %d %d %d", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], version, 0, 0, 0);
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

