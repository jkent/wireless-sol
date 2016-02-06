#include <esp8266.h>
#include "repl.h"
#include "led.h"
#include "button.h"
#include "httpd.h"
#include "httpdespfs.h"
#include "espfs.h"
#include "webpages-espfs.h"
#include "cgi.h"
#include "data.h"
#include "layer.h"
#include "json.h"
#include "preset.h"

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
	{ "/cgi/api", cgiApi, NULL},
	{ "/cgi/layer.json", cgiJson, &json_layer_callback },
	{ "/cgi/layer/background.json", cgiJson, &json_layer_background_callback },
	{ "/cgi/led.json", cgiJson, &json_led_callback },
	{ "/cgi/preset.json", cgiJson, &json_preset_callback },
	{ "/cgi/settings.json", cgiJson, &json_settings_callback },
	{ "*", cgiEspFsHook, NULL },
	{ NULL }
};

bool off_timeout = false;
os_timer_t off_timer;

static void ICACHE_FLASH_ATTR off_timer_cb(void *arg)
{
	os_timer_disarm(&off_timer);
	off_timeout = true;
	preset_apply(0);
	status_save();
}

static void ICACHE_FLASH_ATTR button_down(struct button_data *button)
{
	os_timer_disarm(&off_timer);
	os_timer_setfn(&off_timer, off_timer_cb, NULL);
	os_timer_arm(&off_timer, 300, 1);
	off_timeout = false;
}

static void ICACHE_FLASH_ATTR button_up(struct button_data *button)
{
	os_timer_disarm(&off_timer);
	if (!off_timeout) {
		preset_apply_next();
		status_save();
	}
}

void ICACHE_FLASH_ATTR user_init(void)
{
	system_update_cpu_freq(SYS_CPU_160MHZ);
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	os_printf("\nstarting up\n");
	printf("\nWireless Sol");

	data_init();
	gpio_init();
	led_init();

	button_add(4, button_down, button_up);
	button_init();

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
