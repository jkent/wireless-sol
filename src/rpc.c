#include <esp8266.h>
#include "rpc.h"
#include "httpd.h"
#include "data.h"
#include "led.h"

bool rpc_update = false;

static int ICACHE_FLASH_ATTR layer_enable_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	int id = -1;
	char type;

	if (jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "name") == 0 && type == '"') {
			jsonparse_copy_value(state, name, sizeof(name));
			id = layer_find(name);
		} else if (strcmp(name, "id") == 0 && type == '0') {
			id = jsonparse_get_value_as_int(state);
		} else {
			return RPC_ERROR_PARSE;
		}
	}

	if (id < 0 || id >= layer_count()) {
		return RPC_FAIL;
	}

	if (strcmp(action, "disable") == 0) {
		flash_data.layer_state &= ~(1 << id);
	} else if (strcmp(action, "enable") == 0) {
		flash_data.layer_state |= 1 << id;
	}

	rpc_update = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR layer_insert_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	struct layer layer;
	int id = -1;
	char type;

	memset(&layer, 0, sizeof(layer));

	if (jsonparse_next(state) != '{') {
		os_printf("0\n");
		return RPC_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			os_printf("1\n");
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "name") == 0 && type == '"') {
			jsonparse_copy_value(state, name, sizeof(name));
			strncpy(layer.name, name, sizeof(layer.name));
		} else if (strcmp(name, "id") == 0 && type == '0') {
			id = jsonparse_get_value_as_int(state);
		} else {
			os_printf("2\n");
			return RPC_ERROR_PARSE;
		}
	}

	if (layer.name[0] == '\0') {
		return RPC_FAIL;
	}

	if (layer_find(layer.name) >= 0) {
		return RPC_FAIL;
	}

	if (id < 0) {
		id = layer_count();
	}

	if (!layer_insert(id, &layer)) {
		return RPC_FAIL;
	}

	return RPC_OK;
}

static int ICACHE_FLASH_ATTR layer_move_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	int from, to;
	char type;

	from = to = -1;

	if (jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "name") == 0 && type == '"') {
			jsonparse_copy_value(state, name, sizeof(name));
			from = layer_find(name);
		} else if (strcmp(name, "from") == 0 && type == '0') {
			from = jsonparse_get_value_as_int(state);
		} else if (strcmp(name, "to") == 0 && type == '0') {
			to = jsonparse_get_value_as_int(state);
		} else {
			return RPC_ERROR_PARSE;
		}
	}

	if (!layer_move(from, to)) {
		return RPC_FAIL;
	}

	rpc_update = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR layer_remove_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	int id = -1;
	char type;

	if (jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "name") == 0 && type == '"') {
			jsonparse_copy_value(state, name, sizeof(name));
			id = layer_find(name);
		} else if (strcmp(name, "id") == 0 && type == '0') {
			id = jsonparse_get_value_as_int(state);
		} else {
			return RPC_ERROR_PARSE;
		}
	}

	if (!layer_remove(id, NULL)) {
		return RPC_FAIL;
	}

	rpc_update = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR layer_rename_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	char new[LAYER_NAME_MAX + 1];
	int id = -1;
	char type;
	struct layer *layer;

	new[0] = '\0';

	if (jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "old") == 0 && type == '"') {
			jsonparse_copy_value(state, name, sizeof(name));
			id = layer_find(name);
		} else if (strcmp(name, "id") == 0 && type == '0') {
			id = jsonparse_get_value_as_int(state);
		} else if (strcmp(name, "new") == 0 && type == '"') {
			jsonparse_copy_value(state, new, sizeof(new));
		} else {
			return RPC_ERROR_PARSE;
		}
	}

	if (id < 0 || id >= layer_count() || new[0] == '\0') {
		return RPC_FAIL;
	}

	layer = &flash_data.layers[id];

	if (strcmp(layer->name, new) == 0) {
		return RPC_OK;
	}

	if (layer_find(new) >= 0) {
		return RPC_FAIL;
	}

	strncpy(layer->name, new, sizeof(layer->name));
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR layer_background_set_handler(struct jsonparse_state *state, const char *action)
{
	char name[6];
	int value;
	char type;

	value = flash_data.background;

	if (jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "value") == 0 && type == '0') {
			value = jsonparse_get_value_as_int(state);
			if (value < 0 || value > 255) {
				return RPC_ERROR_PARSE;
			}
		} else {
			return RPC_ERROR_PARSE;
		}
	}

	flash_data.background = value;
	rpc_update = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR led_set_handler(struct jsonparse_state *state, const char *action)
{
	char name[7];
	uint16_t i;
	int value;
	uint8_t values[LED_MAX];
	char type;

	if (jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "values") == 0 && type == '[') {
			i = 0;
			while (true) {
				if ((type = jsonparse_next(state)) == ']') {
					break;
				} else if (type == ',') {
					continue;
				} else if (type != '0') {
					return RPC_ERROR_PARSE;
				}

				if (i >= flash_data.led_count) {
					return RPC_FAIL;
				}

				value = jsonparse_get_value_as_int(state);
				if (value < 0 || value > 255) {
					return RPC_ERROR_PARSE;
				}

				values[i++] = value;
			}

			if (i < flash_data.led_count) {
				return RPC_FAIL;
			}
		} else {
			return RPC_ERROR_PARSE;
		}
	}

	memcpy(led_next, values, flash_data.led_count);
	rpc_update = true;
	return RPC_OK;
}

static int ICACHE_FLASH_ATTR led_mode_set_handler(struct jsonparse_state *state, const char *action)
{
	char name[6];
	uint8_t mode;
	char type;

	mode = flash_data.led_mode;

	if (jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "mode") == 0 && type == '"') {
			jsonparse_copy_value(state, name, sizeof(name));
			if (strcmp(name, "off") == 0) {
				mode = (mode & LED_MODE_FADE) | LED_MODE_OFF;
			} else if (strcmp(name, "layer") == 0) {
				mode = (mode & LED_MODE_FADE) | LED_MODE_LAYER;
			} else if (strcmp(name, "set") == 0) {
				mode = (mode & LED_MODE_FADE) | LED_MODE_SET;
			} else {
				return RPC_FAIL;
			}
		} else if (strcmp(name, "fade") == 0 && type == '"') {
			if (type == 't') {
				mode |= LED_MODE_FADE;
			} else if (type == 'f') {
				mode &= ~LED_MODE_FADE;
			} else {
				return RPC_ERROR_PARSE;
			}
		} else {
			return RPC_ERROR_PARSE;
		}
	}

	flash_data.led_mode = mode;
	rpc_update = true;
	return RPC_OK;
}

static struct rpc_handler handlers[] = {
		{"layer", "disable", layer_enable_handler},
		{"layer", "enable", layer_enable_handler},
		{"layer", "insert", layer_insert_handler},
		{"layer", "move", layer_move_handler},
		{"layer", "remove", layer_remove_handler},
		{"layer", "rename", layer_rename_handler},
		{"layer/background", "set", layer_background_set_handler},
		{"led", "set", led_set_handler},
		{"led/mode", "set", led_mode_set_handler},
		{NULL}
};

int ICACHE_FLASH_ATTR rpc_parse(struct jsonparse_state *state)
{
	char name[7], path[33], action[9];
	struct rpc_handler *handler;
	char type;

	path[0] = action[0] = '\0';

	if (jsonparse_next(state) != '[' || jsonparse_next(state) != '{') {
		return RPC_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return RPC_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "path") == 0 && type == '"') {
			jsonparse_copy_value(state, path, sizeof(path));
		} else if (strcmp(name, "action") == 0 && type == '"') {
			jsonparse_copy_value(state, action, sizeof(action));
		} else {
			return RPC_ERROR_PARSE;
		}
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
	return RPC_FAIL;
}
