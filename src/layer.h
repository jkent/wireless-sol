#ifndef LAYER_H
#define LAYER_H

#include <stdbool.h>

#define RANGE_MAX 4
#define LAYER_NAME_MAX 16
#define LAYER_MAX 8

enum {
	RANGE_TYPE_NONE,
	RANGE_TYPE_SET,
	RANGE_TYPE_COPY,
	RANGE_TYPE_TAPER,
};

struct range {
	uint8_t type;
	uint16_t lb;
	uint16_t ub;
	uint16_t value;
};

struct layer {
	char name[LAYER_NAME_MAX];
	struct range ranges[RANGE_MAX];
};

void ICACHE_FLASH_ATTR layer_update(void);
int8_t ICACHE_FLASH_ATTR layer_find(const char *name);
uint8_t ICACHE_FLASH_ATTR layer_count(void);
bool ICACHE_FLASH_ATTR layer_insert(uint8_t id, struct layer *layer);
bool ICACHE_FLASH_ATTR layer_remove(uint8_t id, struct layer *layer);
bool ICACHE_FLASH_ATTR layer_move(uint8_t from, uint8_t to);

#endif /* LAYER_H */
