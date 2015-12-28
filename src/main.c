#include <esp8266.h>
#include "repl.h"
#include "led.h"
#include "button.h"
#include "httpd.h"
#include "httpdespfs.h"
#include "espfs.h"
#include "webpages-espfs.h"
#include "cgi.h"

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

HttpdBuiltInUrl builtInUrls[] = {
	{ "/", cgiRedirect, "/index.html" },
	{ "/led.json", cgiLedJson, NULL },
	{ "*", cgiEspFsHook, NULL },
	{ NULL }
};

os_timer_t fade_timer;

static void ICACHE_FLASH_ATTR fade_timer_cb(void *arg)
{
	os_timer_arm(&fade_timer, 17, 1);

	if (led_current[0] >= 5) {
		led_current[0] -= 5;
	}
	else {
		led_current[0] = 0;
	}
	memset(led_current, led_current[0], sizeof(led_current));
	led_update();
}

static void ICACHE_FLASH_ATTR button_down(struct button_data *button)
{
	os_timer_disarm(&fade_timer);
	os_timer_setfn(&fade_timer, fade_timer_cb, NULL);
	os_timer_arm(&fade_timer, 200, 1);

	led_current[0] = 255;
	memset(led_current, led_current[0], sizeof(led_current));
	led_update();
}

static void ICACHE_FLASH_ATTR button_up(struct button_data *button)
{
	os_timer_disarm(&fade_timer);
}

void ICACHE_FLASH_ATTR user_init(void)
{
	system_update_cpu_freq(SYS_CPU_160MHZ);

	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	gpio_init();
	led_init();
	button_add(4, button_down, button_up);
	button_init();

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
