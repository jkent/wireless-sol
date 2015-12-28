#include <esp8266.h>
#include "httpd_util.h"

#define MAX_HEAD_LEN 1024
#define MAX_SENDBUFF_LEN 2048

struct HttpdPriv {
	char head[MAX_HEAD_LEN];
	int headPos;
	char *sendBuff;
	int sendBuffLen;
};

int ICACHE_FLASH_ATTR httpdPutchar(int c)
{
	if (current_conn->priv->sendBuffLen >= MAX_SENDBUFF_LEN) {
		return -1;
	}
	*(current_conn->priv->sendBuff + current_conn->priv->sendBuffLen++) =
			(char)c;
	return c;
}
