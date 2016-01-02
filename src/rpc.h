#ifndef RPC_H
#define RPC_H

#include "httpd.h"
#include "jsonparse.h"
#include <stdbool.h>

#define RPC_ERROR_PARSE -1
#define RPC_OK 0
#define RPC_FAIL 1

struct rpc_handler {
	const char *path;
	const char *action;
	int (*func)(struct jsonparse_state *state, const char *action);
};

extern bool rpc_update;

int ICACHE_FLASH_ATTR rpc_parse(struct jsonparse_state *state);

#endif /* RPC_H */
