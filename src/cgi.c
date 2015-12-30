#include <esp8266.h>
#include "httpd.h"
#include "jsontree.h"
#include "jsonparse.h"
#include "rpc.h"

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
	char buff[RPC_MAX_BUFF + 1];
	uint8_t depth;
	uint8_t status;
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

int ICACHE_FLASH_ATTR myHttpdSend(HttpdConnData *conn, const char *data, int len) {
	if (len<0) len=strlen(data);
	if (conn->priv->sendBuffLen+len>MAX_SENDBUFF_LEN) return 0;
	os_memcpy(conn->priv->sendBuff+conn->priv->sendBuffLen, data, len);
	conn->priv->sendBuffLen+=len;
	return 1;
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
	int nbytes;
	char type;

	if (connData->conn == NULL) {
		if (rpc) {
			free(rpc);
		}
		return HTTPD_CGI_DONE;
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
		rpc->status = 1;

		connData->cgiPrivData = rpc;
	}

	if (rpc->status < 0) {
		goto done;
	}

	while (true) {
		nbytes = RPC_MAX_BUFF - rpc->buffLen;
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
				rpc->status = -1;
				goto done;
			}
			if (!rpc_parse(&rpc->state)) {
				rpc->status = 0;
			}
			while (rpc->state.depth > rpc->depth) {
				if (!jsonparse_next(&rpc->state)) {
					rpc->status = -1;
					goto done;
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
				rpc->status = -1;
			}
			goto done;
		}
		if (!rpc_parse(&rpc->state)) {
			rpc->status = 0;
		}
		while (rpc->state.depth > rpc->depth) {
			if (!jsonparse_next(&rpc->state)) {
				rpc->status = -1;
				goto done;
			}
		}
	}

done:
	if (connData->post->received < connData->post->len) {
		return HTTPD_CGI_MORE;
	}

	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "application/json");
	httpdEndHeaders(connData);

	if (rpc->status > 0) {
		httpdSend(connData, "true", -1);
	} else if (rpc->status == 0) {
		httpdSend(connData, "false", -1);
	} else {
		httpdSend(connData, "null", -1);
	}

	free(rpc);
	connData->cgiPrivData = NULL;
	return HTTPD_CGI_DONE;
}
