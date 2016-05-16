#include <esp8266.h>
#include "uart.h"
#include "repl.h"
#include "led.h"
#include "data.h"
#include "layer.h"
#include "util.h"
#include "wol.h"

#define REPL_MAXBUF 79

char repl_buf[REPL_MAXBUF];
unsigned char repl_buflen;

static void ICACHE_FLASH_ATTR repl_write_prompt(void)
{
	uart0_printf("\n> %s", repl_buf);
}

void ICACHE_FLASH_ATTR repl_init(void)
{
	repl_buflen = 0;
	repl_buf[repl_buflen] = '\0';

	uart_task_init();
	repl_write_prompt();
}

static char *ICACHE_FLASH_ATTR repl_parse_args(char **args)
{
	char *arg, *p;
	bool quoted = false;

	/* check end of string */
	if (!**args) {
		return NULL;
	}

	/* check for quote */
	arg = *args;
	if (*arg == '"') {
		quoted = true;
		arg++;
	}

	/* skip until end of argument */
	p = arg;
	if (quoted) {
		while (*p && (*p != '"' || (*(p + 1) && *(p + 1) != ' '))) {
			p++;
		}
		if (*p == '"') {
			*p = ' ';
		}
	}
	else {
		while (*p && *p != ' ') {
			p++;
		}
	}

	/* skip trailing spaces */
	*args = p;
	while (**args == ' ') {
		(*args)++;
	}
	*p = '\0';
	return arg;
}

static void ICACHE_FLASH_ATTR repl_command_data_load(char *args)
{
	config_load();
}

static void ICACHE_FLASH_ATTR repl_command_data_save(char *args)
{
	config_save();
}

static void ICACHE_FLASH_ATTR repl_command_help(char *args)
{
	uart0_printf("\ndata-load");
	uart0_printf("\ndata-save");
	uart0_printf("\nled-set <level>");
	uart0_printf("\nwifi-connect <ssid> [password]");
	uart0_printf("\nwifi-disconnect");
	uart0_printf("\nwifi-status");
	uart0_printf("\nwol-enable [1|0]");
	uart0_printf("\nwol-mac [mac]");
	uart0_printf("\nwol-send <mac>");
}

static void ICACHE_FLASH_ATTR repl_command_led_set(char *args)
{
	const char *level;

	level = repl_parse_args(&args);

	if (!level) {
		uart0_printf("\nlevel is required");
		return;
	}

	memset(led_current, atoi(level), sizeof(led_current));
	led_update();
}

static void ICACHE_FLASH_ATTR repl_command_wifi_connect(char *args)
{
	const char *ssid, *password;
	struct station_config stationConf;

	ssid = repl_parse_args(&args);
	password = repl_parse_args(&args);

	if (!ssid) {
		uart0_printf("\nssid is required");
		return;
	}

	memset(&stationConf, 0, sizeof(stationConf));

	strncpy((char *)&stationConf.ssid, ssid, sizeof(stationConf.ssid));
	if (password) {
		strncpy((char *)&stationConf.password, password,
				sizeof(stationConf.password));
	}

	wifi_set_opmode(STATION_MODE);
	wifi_station_disconnect();
	wifi_station_set_config(&stationConf);
	wifi_station_set_auto_connect(true);
	wifi_station_set_reconnect_policy(true);
	wifi_station_connect();
}

static void ICACHE_FLASH_ATTR repl_command_wifi_disconnect(char *args)
{
	struct station_config stationConf;

	memset(&stationConf, 0, sizeof(stationConf));

	wifi_set_opmode(STATION_MODE);
	wifi_station_set_config(&stationConf);
	wifi_station_disconnect();
}

static void ICACHE_FLASH_ATTR repl_command_wifi_status(char *args)
{
	unsigned char status;
	struct ip_info ip;
	signed char rssi;

	status = wifi_station_get_connect_status();

	switch (status) {
	case STATION_IDLE:
		uart0_printf("\nidle");
		break;
	case STATION_CONNECTING:
		uart0_printf("\nconnecting");
		break;
	case STATION_WRONG_PASSWORD:
		uart0_printf("\nwrong password");
		break;
	case STATION_NO_AP_FOUND:
		uart0_printf("\nap not found");
		break;
	case STATION_CONNECT_FAIL:
		uart0_printf("\nconnect fail");
		break;
	case STATION_GOT_IP:
		wifi_get_ip_info(STATION_IF, &ip);
		rssi = wifi_station_get_rssi();
		uart0_printf("\ngot ip " IPSTR ", rssi %d dBm", IP2STR(&ip), rssi);
		break;
	default:
		uart0_printf("\nunknown");
		break;
	}
}

static void ICACHE_FLASH_ATTR repl_command_wol_enable(char *args)
{
	const char *enable;

	enable = repl_parse_args(&args);

	if (!enable) {
		uart0_printf(config_data.wol_enabled ? "\n1" : "\n0");
		return;
	}

	config_data.wol_enabled = !!atoi(enable);
}

static void ICACHE_FLASH_ATTR repl_command_wol_mac(char *args)
{
	char *mac_str;
	uint8_t mac[6];

	mac_str = repl_parse_args(&args);

	if (!mac_str) {
		uart0_printf("\n" MACSTR, MAC2STR(config_data.wol_mac));
		return;
	}

	if (!parse_mac(mac_str, mac)) {
		uart0_printf("\nmalformed mac address");
	}

	memcpy(config_data.wol_mac, mac, 6);
}

static void ICACHE_FLASH_ATTR repl_command_wol_send(char *args)
{
	char *mac_str;
	uint8_t mac[6];

	mac_str = repl_parse_args(&args);

	if (!mac_str) {
		uart0_printf("\nmac address required");
		return;
	}

	if (!parse_mac(mac_str, mac)) {
		uart0_printf("\nmalformed mac address");
	}

	wol_send(mac);
}

static repl_command commands[] = {
	{ "data-load", repl_command_data_load },
	{ "data-save", repl_command_data_save },
	{ "help", repl_command_help },
	{ "led-set", repl_command_led_set },
	{ "wifi-connect", repl_command_wifi_connect },
	{ "wifi-disconnect", repl_command_wifi_disconnect },
	{ "wifi-status", repl_command_wifi_status },
	{ "wol-enable", repl_command_wol_enable },
	{ "wol-mac", repl_command_wol_mac },
	{ "wol-send", repl_command_wol_send },
	{ NULL } };

static void ICACHE_FLASH_ATTR repl_process_buf(void)
{
	char *cmd, *p, *args = NULL;
	repl_command *command;

	/* remove trailing whitespace */
	while (repl_buflen > 0 && *(repl_buf + repl_buflen - 1) == ' ') {
		repl_buflen--;
		repl_buf[repl_buflen] = '\0';
	}

	/* skip leading whitespace */
	cmd = repl_buf;
	while (*cmd == ' ') {
		cmd++;
	}

	/* lowercase until space */
	p = cmd;
	while (*p && *p != ' ') {
		if (*p >= 'A' && *p <= 'Z') {
			*p += 32;
		}
		p++;
	}

	/* skip whitepace before args */
	args = p;
	while (*args == ' ') {
		args++;
	}

	/* terminate cmd */
	*p = '\0';

	command = commands;
	while (command->name) {
		if (strcmp(cmd, command->name) != 0) {
			command++;
			continue;
		}

		command->handler(args);
		return;
	}

	if (strlen(cmd) > 0) {
		uart0_printf("\nunknown command, see help");
	}
}

void ICACHE_FLASH_ATTR repl_process_char(char c)
{
	if (c == '\b' || c == 127) {
		if (repl_buflen > 0) {
			repl_buflen--;
			repl_buf[repl_buflen] = '\0';
			uart0_putchar('\b');
			uart0_putchar(' ');
			uart0_putchar('\b');
		}
	}
	else if (c >= 32 && c < 127) {
		if (repl_buflen < REPL_MAXBUF - 1) {
			repl_buf[repl_buflen++] = c;
			repl_buf[repl_buflen] = '\0';
			uart0_putchar(c);
		}
	}
	else if (c == '\r') {
		repl_process_buf();
		repl_buflen = 0;
		repl_buf[repl_buflen] = '\0';
		repl_write_prompt();
	}
}
