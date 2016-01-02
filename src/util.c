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
