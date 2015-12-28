#ifndef LAYER_H
#define LAYER_H

#include <stdbool.h>

#define RANGE_MAX 4
#define LAYER_NAME_MAX 16
#define LAYER_MAX 8

enum range_type {
	RANGE_TYPE_NONE,
	RANGE_TYPE_SET,
	RANGE_TYPE_COPY,
	RANGE_TYPE_TAPER,
	RANGE_TYPE_MAX
};

struct range {
	enum range_type type;
	uint16_t lb;
	uint16_t ub;
	uint16_t value;
};

struct layer {
	char name[LAYER_NAME_MAX];
	bool visible;
	struct range ranges[RANGE_MAX];
};

void ICACHE_FLASH_ATTR layer_update(void);

#endif /* LAYER_H */
