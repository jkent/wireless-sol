#include <esp8266.h>
#include "repl.h"
#include "led.h"
#include "data.h"
#include "layer.h"

#define REPL_MAXBUF 79

char repl_buf[REPL_MAXBUF];
unsigned char repl_buflen;

static void ICACHE_FLASH_ATTR repl_write_prompt(void)
{
	printf("\n> %s", repl_buf);
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
	data_load();
}

static void ICACHE_FLASH_ATTR repl_command_data_save(char *args)
{
	data_save();
}

static void ICACHE_FLASH_ATTR repl_command_help(char *args)
{
	printf("\ndata-load");
	printf("\ndata-save");
	printf("\nled-set <level>");
	printf("\nwifi-connect <ssid> [password]");
	printf("\nwifi-disconnect");
	printf("\nwifi-status");
}

static void ICACHE_FLASH_ATTR repl_command_led_set(char *args)
{
	const char *level;

	level = repl_parse_args(&args);

	if (!level) {
		printf("\nlevel is required");
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
		printf("\nssid is required");
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
		printf("\nidle");
		break;
	case STATION_CONNECTING:
		printf("\nconnecting");
		break;
	case STATION_WRONG_PASSWORD:
		printf("\nwrong password");
		break;
	case STATION_NO_AP_FOUND:
		printf("\nap not found");
		break;
	case STATION_CONNECT_FAIL:
		printf("\nconnect fail");
		break;
	case STATION_GOT_IP:
		wifi_get_ip_info(STATION_IF, &ip);
		rssi = wifi_station_get_rssi();
		printf("\ngot ip " IPSTR ", rssi %d dBm", IP2STR(&ip), rssi);
		break;
	default:
		printf("\nunknown");
		break;
	}
}

static repl_command commands[] = {
	{ "data-load", repl_command_data_load },
	{ "data-save", repl_command_data_save },
	{ "help", repl_command_help },
	{ "led-set", repl_command_led_set },
	{ "wifi-connect", repl_command_wifi_connect },
	{ "wifi-disconnect", repl_command_wifi_disconnect },
	{ "wifi-status", repl_command_wifi_status },
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
		printf("\nunknown command, see help");
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
