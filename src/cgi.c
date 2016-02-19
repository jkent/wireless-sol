#include <esp8266.h>
#include "api.h"
#include "httpd.h"
#include "jsontree.h"
#include "jsonparse.h"
#include "led.h"
#include "data.h"

#define MAX_HEAD_LEN 1024
#define MAX_POST 1024
#define MAX_SENDBUFF_LEN 2048

#define API_MIN_CHUNK (MAX_POST / 2)
#define API_MAX_BUFF (MAX_POST + API_MIN_CHUNK)

struct HttpdPriv {
	char head[MAX_HEAD_LEN];
	int headPos;
	char *sendBuff;
	int sendBuffLen;
};

struct ApiData {
	struct jsonparse_state state;
	int buffLen;
	char buff[API_MAX_BUFF];
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

		httpdDisableTransferEncoding(connData);
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

int ICACHE_FLASH_ATTR cgiApi(HttpdConnData *connData)
{
	struct ApiData *api = (struct ApiData *)connData->cgiData;
	int nbytes, status;
	char type;

	if (connData->conn == NULL) {
		goto done;
	}

	if (connData->requestType != HTTPD_METHOD_POST) {
		httpdDisableTransferEncoding(connData);
		httpdStartResponse(connData, 501);
		httpdHeader(connData, "Content-Type", "text/html");
		httpdEndHeaders(connData);
		return HTTPD_CGI_DONE;
	}

	if (connData->post->received <= MAX_POST) {
		api = malloc(sizeof(struct ApiData));
		jsonparse_setup(&api->state, api->buff, MAX_POST);
		api->buffLen = 0;
		api->depth = 0;
		api->status = API_OK;

		connData->cgiData = api;
	}

	if (api->status < API_OK) {
		goto finish;
	}

	while (true) {
		nbytes = API_MAX_BUFF - api->buffLen - 1;
		nbytes = connData->post->buffLen < nbytes ? connData->post->buffLen : nbytes;
		memcpy(&api->buff[api->buffLen], connData->post->buff, nbytes);
		api->buffLen += nbytes;
		memmove(connData->post->buff, &connData->post->buff[nbytes], connData->post->buffLen - nbytes);
		connData->post->buffLen -= nbytes;
		if (connData->post->buffLen == 0) {
			if (connData->post->received < connData->post->len) {
				return HTTPD_CGI_MORE;
			}
			break;
		}
		while (api->state.pos < API_MIN_CHUNK) {
			type = jsonparse_next(&api->state);
			if (type == ',') {
				/* do nothing */
			} else if (type == '[') {
				api->depth = api->state.depth;
			} else {
				api->status = API_ERROR_PARSE;
				goto finish;
			}
			if ((status = api_parse(&api->state)) != API_OK) {
				api->status = status;
				if (status < API_OK) {
					goto finish;
				}
			}
			while (api->state.depth > api->depth) {
				if (!jsonparse_next(&api->state)) {
					api->status = API_ERROR_PARSE;
					goto finish;
				}
			}
		}
		memmove(api->buff, &api->buff[api->state.pos], api->buffLen - api->state.pos);
		api->buffLen -= api->state.pos;
		api->state.pos = 0;
	}

	api->buff[api->buffLen] = '\0';
	api->state.len = api->buffLen;
	while (true) {
		type = jsonparse_next(&api->state);
		if (type == ',') {
			/* do nothing */
		} else if (type == '[') {
			api->depth = api->state.depth;
		} else {
			if (api->state.error != JSON_ERROR_OK) {
				api->status = API_ERROR_PARSE;
			}
			goto finish;
		}
		if ((status = api_parse(&api->state)) != API_OK) {
			api->status = status;
			if (status < API_OK) {
				goto finish;
			}
		}
		while (api->state.depth > api->depth) {
			if (!jsonparse_next(&api->state)) {
				api->status = API_ERROR_PARSE;
				goto finish;
			}
		}
	}

finish:
	if (connData->post->received < connData->post->len) {
		return HTTPD_CGI_MORE;
	}

	if (api->status < API_OK) {
		httpdDisableTransferEncoding(connData);
		httpdStartResponse(connData, 500);
		httpdHeader(connData, "Content-Type", "text/html");
		httpdEndHeaders(connData);
		goto done;
	}

	httpdDisableTransferEncoding(connData);
	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "application/json");
	httpdEndHeaders(connData);
	httpdSend(connData, api->status == API_OK ? "true" : "false", -1);

	if (status_dirty) {
		status_save();
	}

done:
	free(api);
	connData->cgiData = NULL;
	return HTTPD_CGI_DONE;
}
