#include <esp8266.h>
#include "util.h"

uint32_t ICACHE_FLASH_ATTR bit_remove(uint8_t id, uint32_t data)
{
	return ((data & 0xffffffff << (id + 1)) >> 1) | (data & ((1 << id) - 1));
}

uint32_t ICACHE_FLASH_ATTR bit_insert(uint8_t id, uint32_t data)
{
	return ((data & 0xffffffff << id) << 1) | (data & ((1 << id) - 1));
}

bool ICACHE_FLASH_ATTR parse_mac(char *s, uint8_t *mac)
{
	int i, n;

	for (i = 0; i < 6; i++) {
		if (i > 0 && *s++ != ':') {
			return false;
		}
		n = strtol(s, &s, 16);
		if (n < 0 || n > 255) {
			return false;
		}
		mac[i] = n;
	}

	if (*s != '\0') {
		return false;
	}

	return true;
}
