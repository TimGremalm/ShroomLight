#include "shroomlistener.h"

#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/netif.h"

#include "esp_log.h"

static const char *TAG = "Shroom Listener";

void shroomlistenertask(void *pvParameters) {
	shroomlistener_config_t listener_config = *(shroomlistener_config_t *) pvParameters;

	ESP_LOGI(TAG, "Open server %d", listener_config.test);

	struct netconn *conn;
	err_t err;

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

	ip_addr_t multiaddr;
	//IP4_ADDR(&multiaddr, 239, 255, 0, 1); //IPv4 local scope multicast
	IP_ADDR4(&multiaddr, 239, 255, 0, 1); //IPv4 local scope multicast

	err = netconn_join_leave_group(conn, &multiaddr, &netif_default->ip_addr, NETCONN_JOIN);
	if(err != ERR_OK) {
		ESP_LOGE(TAG, "Join Multicast Group. err=%d", err);
		return;
	}

	ESP_LOGI(TAG, "Listening for connections");

	while(1) {
		struct netbuf *buf;

		err = netconn_recv(conn, &buf);
		if(err != ERR_OK) {
			ESP_LOGE(TAG, "Failed to receive packet. err=%d", err);
			continue;
		}
		ESP_LOGI(TAG, "Received packet");

		netbuf_delete(buf);
	}
}

