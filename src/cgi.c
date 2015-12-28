#include <esp8266.h>
#include "httpd.h"
#include "json/jsontree.h"

#define MAX_HEAD_LEN 1024
#define MAX_SENDBUFF_LEN 2048

struct HttpdPriv {
	char head[MAX_HEAD_LEN];
	int headPos;
	char *sendBuff;
	int sendBuffLen;
};

static HttpdConnData *currentConnData;

static int ICACHE_FLASH_ATTR httpdPutchar(int c)
{
	if (c == '\n') {
		return c;
	}
	if (currentConnData->priv->sendBuffLen >= MAX_SENDBUFF_LEN) {
		return -1;
	}
	*(currentConnData->priv->sendBuff + currentConnData->priv->sendBuffLen++) =
			(char)c;
	return c;
}

int ICACHE_FLASH_ATTR cgiJsonGet(HttpdConnData *connData)
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

	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiJson(HttpdConnData *connData)
{
	if (connData->requestType == HTTPD_METHOD_GET) {
		return cgiJsonGet(connData);
	}

	return HTTPD_CGI_NOTFOUND;
}
