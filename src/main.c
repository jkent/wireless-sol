#include <esp8266.h>
#include "repl.h"
#include "led.h"
#include "httpd.h"
#include "httpdespfs.h"
#include "espfs.h"
#include "webpages-espfs.h"

//#define SHOW_HEAP_USE

static struct mdns_info mdns_info;

#ifdef SHOW_HEAP_USE
static ETSTimer prHeapTimer;

static void ICACHE_FLASH_ATTR
prHeapTimerCb(void *arg)
{
	printf("Heap: %ld\n", (unsigned long)system_get_free_heap_size());
}
#endif

static void ICACHE_FLASH_ATTR wifi_handle_event(System_Event_t *evt)
{
	switch (evt->event) {
	case EVENT_STAMODE_DISCONNECTED:
		espconn_mdns_disable();
		espconn_mdns_close();
		break;

	case EVENT_STAMODE_GOT_IP:
		mdns_info.host_name = wifi_station_get_hostname();
		mdns_info.ipAddr = evt->event_info.got_ip.ip.addr;
		mdns_info.server_name = "http";
		mdns_info.server_port = 80;
		mdns_info.txt_data[0] = "api=sol";
		espconn_mdns_init(&mdns_info);
		espconn_mdns_enable();
		break;
	}
}

HttpdBuiltInUrl builtInUrls[] = { { "/", cgiRedirect, "/index.html" }, { "*",
		cgiEspFsHook, NULL }, { NULL, NULL, NULL } };

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	gpio_init();
	led_init();

	printf("\nESPLED controller");
	repl_init();

	wifi_set_event_handler_cb(wifi_handle_event);

	espFsInit((void *)webpages_espfs_start);
	httpdInit(builtInUrls, 80);

#ifdef SHOW_HEAP_USE
	os_timer_disarm(&prHeapTimer);
	os_timer_setfn(&prHeapTimer, prHeapTimerCb, NULL);
	os_timer_arm(&prHeapTimer, 3000, 1);
#endif

}

#ifndef USE_OPENSDK
void ICACHE_FLASH_ATTR
user_rf_pre_init(void)
{
}
#endif
