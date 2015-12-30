#ifndef CGI_H
#define CGI_H

int ICACHE_FLASH_ATTR cgiJson(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiRpc(HttpdConnData *connData);

#endif /* CGI_H */
