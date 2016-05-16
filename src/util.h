#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

uint32_t ICACHE_FLASH_ATTR bit_remove(uint8_t id, uint32_t data);
uint32_t ICACHE_FLASH_ATTR bit_insert(uint8_t id, uint32_t data);
bool ICACHE_FLASH_ATTR parse_mac(char *s, uint8_t *mac);

#endif /* UTIL_H */
