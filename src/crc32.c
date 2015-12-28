#include <esp8266.h>
#include "crc32.h"

unsigned int ICACHE_FLASH_ATTR crc32(uint8_t *data, size_t len)
{
	const unsigned int g0 = 0xEDB88320, g1 = g0 >> 1, g2 = g0 >> 2,
			g3 = g0 >> 3, g4 = g0 >> 4, g5 = g0 >> 5, g6 = (g0 >> 6) ^ g0,
			g7 = ((g0 >> 6) ^ g0) >> 1;

	int crc = 0xFFFFFFFF;
	while (len--) {
		crc = crc ^ *(data++);
		unsigned int c = ((crc << 31 >> 31) & g7) ^ ((crc << 30 >> 31) & g6) ^
				((crc << 29 >> 31) & g5) ^ ((crc << 28 >> 31) & g4) ^
				((crc << 27 >> 31) & g3) ^ ((crc << 26 >> 31) & g2) ^
				((crc << 25 >> 31) & g1) ^ ((crc << 24 >> 31) & g0);
		crc = ((unsigned)crc >> 8) ^ c;
	}
	return ~crc;
}
