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
uint version = 2;

void shroomlistenertask(void *pvParameters) {
	shroomlistener_config_t listener_config = *(shroomlistener_config_t *) pvParameters;

	ESP_LOGI(TAG, "Open server %d", listener_config.test);

	err_t err;
	esp_read_mac(mac, ESP_MAC_WIFI_STA);

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

		//Copy message for parsing
		char message[buf->p->tot_len];
		strcpy(message, buf->p->payload);

		//Parse first space, the command
		char * pch;
		pch = strtok(message, " ");

		if (strncmp(pch, "restart", 7) == 0) {
			ESP_LOGI(TAG, "Restart");
			esp_restart();
		}
		if (strncmp(pch, "information", 11) == 0) {
			ESP_LOGI(TAG, "Information");
			shroom_send_info();
		}
		if (strncmp(pch, "OTA", 3) == 0) {
			//Next separator
			pch = strtok(NULL, " ");
			if (pch != NULL) {
				ESP_LOGI(TAG, "OTA URL: %s", pch);
				ota_start(pch);
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

