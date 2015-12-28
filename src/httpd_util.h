#ifndef HTTPD_UTIL
#define HTTPD_UTIL

#include "httpd.h"

HttpdConnData *current_conn;

int ICACHE_FLASH_ATTR httpdPutchar(int c);

#endif /* HTTPD_UTIL */
