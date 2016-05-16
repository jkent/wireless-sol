#include <esp8266.h>
#include "wol.h"

static struct espconn wol_espconn;

void wol_init(void)
{
	wifi_set_broadcast_if(STATION_MODE);

	wol_espconn.type = ESPCONN_UDP;
	wol_espconn.proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	wol_espconn.proto.udp->local_port = espconn_port();

	const uint8_t remote_ip[4] = {255, 255, 255, 255};
	memcpy(wol_espconn.proto.udp->remote_ip, remote_ip, 4);
	wol_espconn.proto.udp->remote_port = 9;

	espconn_create(&wol_espconn);
}

void wol_cleanup(void)
{
	os_free(wol_espconn.proto.udp);
}

void wol_send(const uint8_t *mac)
{
	uint8_t pkt[102];
	uint8_t *p = pkt;

	memset(p, 255, 6);

	for (int i = 0; i < 16; i++) {
		p += 6;
		memcpy(p, mac, 6);
	}

	const uint8_t remote_ip[4] = {255, 255, 255, 255};
	memcpy(wol_espconn.proto.udp->remote_ip, remote_ip, 4);
	wol_espconn.proto.udp->remote_port = 9;

	espconn_sent(&wol_espconn, pkt, sizeof(pkt));
}
