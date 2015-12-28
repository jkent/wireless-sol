#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include "led.h"
#include "layer.h"

struct flash_data {
	enum led_mode led_mode;
	uint16_t led_count;
	uint8_t layer_background;
	struct layer layers[LAYER_MAX];
};

extern struct flash_data flash_data;

void ICACHE_FLASH_ATTR data_init(void);

#endif /* DATA_H */
