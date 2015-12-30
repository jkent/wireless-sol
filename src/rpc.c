#include <esp8266.h>
#include "rpc.h"
#include "httpd.h"

static bool ICACHE_FLASH_ATTR layer_enable_handler(struct jsonparse_state *state, const char *action)
{
	bool enable = (strcmp(action, "enable") == 0);
	char name[17];

	if (jsonparse_next(state) != '"') {
		return false;
	}

	jsonparse_copy_value(state, name, sizeof(name));

	os_printf("%s = %s\n", name, enable ? "true" : "false");

	return true;
}

static struct rpc_handler handlers[] = {
		{"layer", "disable", layer_enable_handler},
		{"layer", "enable", layer_enable_handler},
		{NULL}
};

bool ICACHE_FLASH_ATTR rpc_parse(struct jsonparse_state *state)
{
	char key[17], path[17], action[17];
	bool have_path, have_action;
	struct rpc_handler *handler;

	have_path = have_action = false;

	if (jsonparse_next(state) != '[' || jsonparse_next(state) != '{') {
		return false;
	}

	do {
		if (jsonparse_next(state) != 'N') {
			return false;
		}
		jsonparse_copy_value(state, key, sizeof(key));

		if (jsonparse_next(state) != '"') {
			return false;
		}
		if (strcmp(key, "path") == 0) {
			jsonparse_copy_value(state, path, sizeof(path));
			have_path = true;
		} else if (strcmp(key, "action") == 0) {
			jsonparse_copy_value(state, action, sizeof(action));
			have_action = true;
		} else {
			return false;
		}
	} while (jsonparse_next(state) == ',');

	if (!have_path || !have_action) {
		return false;
	}

	if (jsonparse_next(state) != ',') {
		return false;
	}

	handler = handlers;
	while (handler->func) {
		if (strcmp(path, handler->path) == 0 && strcmp(action, handler->action) == 0) {
			return handler->func(state, action);
		}
		handler++;
	}
	return false;
}
