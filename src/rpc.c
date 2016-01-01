#include <esp8266.h>
#include "rpc.h"
#include "httpd.h"
#include "data.h"
#include "led.h"

static int ICACHE_FLASH_ATTR layer_background_handler(struct jsonparse_state *state, const char *action)
{
	int value;

	if (jsonparse_next(state) != '0') {
		return RPC_ERROR_PARSE;
	}

	value = jsonparse_get_value_as_int(state);
	if (value < 0 || value > 255) {
		return RPC_FAIL;
	}

	flash_data.background = value;
	flash_data.led_mode = LED_MODE_LAYER;
	led_update_required = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR layer_enable_handler(struct jsonparse_state *state, const char *action)
{
	char name[17];
	int8_t id;

	if (jsonparse_next(state) != '"') {
		return RPC_ERROR_PARSE;
	}

	jsonparse_copy_value(state, name, sizeof(name));
	if ((id = layer_find(name)) < 0) {
		return RPC_FAIL;
	}

	if (strcmp(action, "disable") == 0) {
		flash_data.layer_state &= ~(1 << id);
	} else if (strcmp(action, "enable") == 0) {
		flash_data.layer_state |= 1 << id;
	} else {
		return RPC_FAIL;
	}

	led_update_required = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR led_set_handler(struct jsonparse_state *state, const char *action)
{
	uint8_t type;
	uint16_t i;

	if (jsonparse_next(state) != '[') {
		return RPC_ERROR_PARSE;
	}

	for (i = 0; i < flash_data.led_count; i++) {
		type = jsonparse_next(state);
		if (type == '[' || type == ',') {
			type = jsonparse_next(state);
		} else if (type == ']') {
			break;
		}

		if (type != '0') {
			return RPC_ERROR_PARSE;
		}

		led_next[i] = jsonparse_get_value_as_int(state);
	}

	if (i < flash_data.led_count) {
		return RPC_FAIL;
	}

	flash_data.led_mode = LED_MODE_SET;
	led_update_required = true;

	return RPC_OK;
}

static int ICACHE_FLASH_ATTR led_mode_handler(struct jsonparse_state *state, const char *action)
{
	if (jsonparse_next(state) != '"') {
		return RPC_ERROR_PARSE;
	}

	if (jsonparse_strcmp_value(state, "off") == 0) {
		flash_data.led_mode = LED_MODE_OFF;
	} else if (jsonparse_strcmp_value(state, "off fade") == 0) {
		flash_data.led_mode = LED_MODE_OFF_FADE;
	} else if (jsonparse_strcmp_value(state, "set") == 0) {
		flash_data.led_mode = LED_MODE_SET;
	} else if (jsonparse_strcmp_value(state, "set fade") == 0) {
		flash_data.led_mode = LED_MODE_SET_FADE;
	} else if (jsonparse_strcmp_value(state, "layer") == 0) {
		flash_data.led_mode = LED_MODE_LAYER;
	} else if (jsonparse_strcmp_value(state, "layer fade") == 0) {
		flash_data.led_mode = LED_MODE_LAYER_FADE;
	} else {
		return RPC_FAIL;
	}

	led_update_required = true;
	return RPC_OK;
}

static struct rpc_handler handlers[] = {
		{"layer", "background", layer_background_handler},
		{"layer", "disable", layer_enable_handler},
		{"layer", "enable", layer_enable_handler},
		{"led", "set", led_set_handler},
		{"led", "mode", led_mode_handler},
		{NULL}
};

int ICACHE_FLASH_ATTR rpc_parse(struct jsonparse_state *state)
{
	char key[17], path[17], action[17];
	bool have_path, have_action;
	struct rpc_handler *handler;

	have_path = have_action = false;

	if (jsonparse_next(state) != '[' || jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	do {
		if (jsonparse_next(state) != 'N') {
			return RPC_ERROR_PARSE;
		}
		jsonparse_copy_value(state, key, sizeof(key));

		if (jsonparse_next(state) != '"') {
			return RPC_ERROR_PARSE;
		}
		if (strcmp(key, "path") == 0) {
			jsonparse_copy_value(state, path, sizeof(path));
			have_path = true;
		} else if (strcmp(key, "action") == 0) {
			jsonparse_copy_value(state, action, sizeof(action));
			have_action = true;
		} else {
			return RPC_FAIL;
		}
	} while (jsonparse_next(state) == ',');

	if (!have_path || !have_action) {
		return RPC_FAIL;
	}

	if (jsonparse_next(state) != ',') {
		return RPC_ERROR_PARSE;
	}

	handler = handlers;
	while (handler->func) {
		if (strcmp(path, handler->path) == 0 && strcmp(action, handler->action) == 0) {
			return handler->func(state, action);
		}
		handler++;
	}
	return RPC_ERROR_PARSE;
}
