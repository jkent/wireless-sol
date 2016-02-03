#ifndef API_H
#define API_H

#include "httpd.h"
#include "jsonparse.h"
#include <stdbool.h>

#define API_ERROR_PARSE -1
#define API_OK 0
#define API_FAIL 1

struct api_handler {
	const char *path;
	const char *action;
	int (*func)(struct jsonparse_state *state, const char *action);
};

extern bool needs_layer_update;
extern bool needs_led_update;

int ICACHE_FLASH_ATTR api_parse(struct jsonparse_state *state);

#endif /* API_H */
