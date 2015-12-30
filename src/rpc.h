#ifndef RPC_H
#define RPC_H

#include "httpd.h"
#include "jsonparse.h"
#include <stdbool.h>

struct rpc_handler {
	const char *path;
	const char *action;
	bool (*func)(struct jsonparse_state *state, const char *action);
};

bool ICACHE_FLASH_ATTR rpc_parse(struct jsonparse_state *state);

#endif /* RPC_H */
