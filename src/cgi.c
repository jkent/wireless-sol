#include <esp8266.h>
#include "httpd.h"
#include "httpd_util.h"
#include "json_led.h"

int ICACHE_FLASH_ATTR cgiLedJson(HttpdConnData *connData)
{
	if (connData->conn == NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "application/json");
	httpdEndHeaders(connData);

	current_conn = connData;
	json_led_status(httpdPutchar);
	return HTTPD_CGI_DONE;
}
