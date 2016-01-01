#include <esp8266.h>
#include "httpd.h"
#include "jsontree.h"
#include "jsonparse.h"
#include "rpc.h"
#include "led.h"
#include "data.h"

#define MAX_HEAD_LEN 1024
#define MAX_POST 1024
#define MAX_SENDBUFF_LEN 2048

#define RPC_MIN_CHUNK (MAX_POST / 2)
#define RPC_MAX_BUFF (MAX_POST + RPC_MIN_CHUNK)

struct HttpdPriv {
	char head[MAX_HEAD_LEN];
	int headPos;
	char *sendBuff;
	int sendBuffLen;
};

struct RpcData {
	struct jsonparse_state state;
	int buffLen;
	char buff[RPC_MAX_BUFF];
	uint8_t depth;
	int8_t status;
};

static HttpdConnData *currentConnData;

static int ICACHE_FLASH_ATTR httpdPutchar(int c)
{
	if (currentConnData->priv->sendBuffLen >= MAX_SENDBUFF_LEN) {
		return -1;
	}
	*(currentConnData->priv->sendBuff + currentConnData->priv->sendBuffLen++) =
			(char)c;
	return c;
}

int ICACHE_FLASH_ATTR cgiJson(HttpdConnData *connData)
{
	struct jsontree_context *json = (struct jsontree_context *)connData->cgiData;
	char buf[6];

	if (connData->conn == NULL) {
		if (json) {
			free(json);
		}
		return HTTPD_CGI_DONE;
	}

	currentConnData = connData;

	if (json == NULL) {
		json = malloc(sizeof(struct jsontree_context));
		jsontree_setup(json, (struct jsontree_value *)connData->cgiArg, httpdPutchar);

		if (httpdFindArg(connData->getArgs, "id", buf, sizeof(buf)) > 0) {
			json->index[JSONTREE_MAX_DEPTH - 1] = atoi(buf);
		}
		else {
			json->index[JSONTREE_MAX_DEPTH - 1] = 65535;
		}

		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", "application/json");
		httpdEndHeaders(connData);
		connData->cgiData = json;
	}

	while (jsontree_print_next(json) && json->path <= json->depth) {
		if (connData->priv->sendBuffLen > MAX_SENDBUFF_LEN - (MAX_SENDBUFF_LEN / 4)) {
			return HTTPD_CGI_MORE;
		}
	}

	free(json);
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiRpc(HttpdConnData *connData)
{
	struct RpcData *rpc = (struct RpcData *)connData->cgiPrivData;
	int nbytes, status;
	char type;

	if (connData->conn == NULL) {
		goto done;
	}

	if (connData->requestType != HTTPD_METHOD_POST) {
		httpdStartResponse(connData, 501);
		httpdHeader(connData, "Content-Type", "text/html");
		httpdEndHeaders(connData);
		return HTTPD_CGI_DONE;
	}

	if (connData->post->received <= MAX_POST) {
		rpc = malloc(sizeof(struct RpcData));
		jsonparse_setup(&rpc->state, rpc->buff, MAX_POST);
		rpc->buffLen = 0;
		rpc->depth = 0;
		rpc->status = RPC_OK;

		connData->cgiPrivData = rpc;
	}

	if (rpc->status < RPC_OK) {
		goto finish;
	}

	while (true) {
		nbytes = RPC_MAX_BUFF - rpc->buffLen - 1;
		nbytes = connData->post->buffLen < nbytes ? connData->post->buffLen : nbytes;
		memcpy(&rpc->buff[rpc->buffLen], connData->post->buff, nbytes);
		rpc->buffLen += nbytes;
		memmove(connData->post->buff, &connData->post->buff[nbytes], connData->post->buffLen - nbytes);
		connData->post->buffLen -= nbytes;
		if (connData->post->buffLen == 0) {
			if (connData->post->received < connData->post->len) {
				return HTTPD_CGI_MORE;
			}
			break;
		}
		while (rpc->state.pos < RPC_MIN_CHUNK) {
			type = jsonparse_next(&rpc->state);
			if (type == ',') {
				/* do nothing */
			} else if (type == '[') {
				rpc->depth = rpc->state.depth;
			} else {
				rpc->status = RPC_ERROR_PARSE;
				goto finish;
			}
			if ((status = rpc_parse(&rpc->state)) != RPC_OK) {
				rpc->status = status;
				if (status < RPC_OK) {
					goto finish;
				}
			}
			while (rpc->state.depth > rpc->depth) {
				if (!jsonparse_next(&rpc->state)) {
					rpc->status = RPC_ERROR_PARSE;
					goto finish;
				}
			}
		}
		memmove(rpc->buff, &rpc->buff[rpc->state.pos], rpc->buffLen - rpc->state.pos);
		rpc->buffLen -= rpc->state.pos;
		rpc->state.pos = 0;
	}

	rpc->buff[rpc->buffLen] = '\0';
	rpc->state.len = rpc->buffLen;
	while (true) {
		type = jsonparse_next(&rpc->state);
		if (type == ',') {
			/* do nothing */
		} else if (type == '[') {
			rpc->depth = rpc->state.depth;
		} else {
			if (rpc->state.error != JSON_ERROR_OK) {
				rpc->status = RPC_ERROR_PARSE;
			}
			goto finish;
		}
		if ((status = rpc_parse(&rpc->state)) != RPC_OK) {
			rpc->status = status;
			if (status < RPC_OK) {
				goto finish;
			}
		}
		while (rpc->state.depth > rpc->depth) {
			if (!jsonparse_next(&rpc->state)) {
				rpc->status = RPC_ERROR_PARSE;
				goto finish;
			}
		}
	}

finish:
	if (connData->post->received < connData->post->len) {
		return HTTPD_CGI_MORE;
	}

	if (rpc->status < RPC_OK) {
		httpdStartResponse(connData, 500);
		httpdHeader(connData, "Content-Type", "text/html");
		httpdEndHeaders(connData);
		goto done;
	}

	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "application/json");
	httpdEndHeaders(connData);
	httpdSend(connData, rpc->status == RPC_OK ? "true" : "false", -1);

	if (led_update_required) {
		led_update_required = false;
		if ((flash_data.led_mode & ~LED_MODE_FADE) == LED_MODE_OFF) {
			memset(led_next, 0, flash_data.led_count);
		} else if ((flash_data.led_mode & ~LED_MODE_FADE) == LED_MODE_LAYER) {
			layer_update();
		}

		if (0 && (flash_data.led_mode & LED_MODE_FADE)) {
			/* TODO: implement & start fade timer */
		} else {
			memcpy(led_current, led_next, flash_data.led_count);
			led_update();
		}
	}

done:
	free(rpc);
	connData->cgiPrivData = NULL;
	return HTTPD_CGI_DONE;
}
