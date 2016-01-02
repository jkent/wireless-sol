#include <esp8266.h>
#include "rpc.h"
#include "httpd.h"
#include "data.h"
#include "led.h"

bool rpc_update = false;

static int ICACHE_FLASH_ATTR layer_enable_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	int8_t id;

	if (jsonparse_next(state) != '"') {
		return RPC_ERROR_PARSE;
	}

	jsonparse_copy_value(state, name, sizeof(name));
	if ((id = layer_find(name)) < 0) {
		return RPC_ERROR_PARSE;
	}

	if (strcmp(action, "disable") == 0) {
		flash_data.layer_state &= ~(1 << id);
	} else if (strcmp(action, "enable") == 0) {
		flash_data.layer_state |= 1 << id;
	}

	rpc_update = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR layer_move_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	int8_t from, to;
	char type;

	from = to = -1;

	if (jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	do {
		if (jsonparse_next(state) != 'N') {
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (from < 0 && strcmp(name, "name") == 0 && type == '"') {
			jsonparse_copy_value(state, name, sizeof(name));
			if ((from = layer_find(name)) < 0) {
				return RPC_ERROR_PARSE;
			}
		} else if (to < 0 && strcmp(name, "id") == 0 && type == '0') {
			if ((to = jsonparse_get_value_as_int(state)) < 0) {
				return RPC_ERROR_PARSE;
			}
		} else {
			return RPC_ERROR_PARSE;
		}
	} while ((type = jsonparse_next(state)) == ',');

	if (type != '}') {
		return RPC_ERROR_PARSE;
	}

	if (from < 0 || to < 0) {
		return RPC_ERROR_PARSE;
	}

	if (!layer_move(from, to)) {
		return RPC_FAIL;
	}

	rpc_update = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR layer_background_set_handler(struct jsonparse_state *state, const char *action)
{
	int value;

	if (jsonparse_next(state) != '0') {
		return RPC_ERROR_PARSE;
	}

	value = jsonparse_get_value_as_int(state);
	if (value < 0 || value > 255) {
		return RPC_ERROR_PARSE;
	}

	flash_data.background = value;
	rpc_update = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR led_set_handler(struct jsonparse_state *state, const char *action)
{
	uint8_t type;
	uint16_t i;
	int value;

	if (jsonparse_next(state) != '[') {
		return RPC_ERROR_PARSE;
	}

	i = 0;
	do {
		if (jsonparse_next(state) != '0') {
			return RPC_ERROR_PARSE;
		}

		value = jsonparse_get_value_as_int(state);
		if (value < 0 || value > 255) {
			return RPC_ERROR_PARSE;
		}
		led_next[i++] = value;

		if (i >= flash_data.led_count) {
			break;
		}
	} while ((type = jsonparse_next(state)) == ',');

	if (type != ']') {
		return RPC_ERROR_PARSE;
	}

	if (i < flash_data.led_count) {
		return RPC_FAIL;
	}

	rpc_update = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR led_mode_set_handler(struct jsonparse_state *state, const char *action)
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

	rpc_update = true;
	return RPC_OK;
}

static struct rpc_handler handlers[] = {
		{"layer", "disable", layer_enable_handler},
		{"layer", "enable", layer_enable_handler},
		{"layer", "move", layer_move_handler},
		{"layer/background", "set", layer_background_set_handler},
		{"led", "set", led_set_handler},
		{"led/mode", "set", led_mode_set_handler},
		{NULL}
};

int ICACHE_FLASH_ATTR rpc_parse(struct jsonparse_state *state)
{
	char name[7], path[33], action[9];
	bool have_path, have_action;
	struct rpc_handler *handler;
	char type;

	have_path = have_action = false;

	if (jsonparse_next(state) != '[' || jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	do {
		if (jsonparse_next(state) != 'N') {
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (!have_path && strcmp(name, "path") == 0 && type == '"') {
			jsonparse_copy_value(state, path, sizeof(path));
			have_path = true;
		} else if (!have_action && strcmp(name, "action") == 0 && type == '"') {
			jsonparse_copy_value(state, action, sizeof(action));
			have_action = true;
		} else {
			return RPC_ERROR_PARSE;
		}
	} while ((type = jsonparse_next(state)) == ',');

	if (type != '}' || jsonparse_next(state) != ',') {
		return RPC_ERROR_PARSE;
	}

	if (!have_path || !have_action) {
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
