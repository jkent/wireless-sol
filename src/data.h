#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include "led.h"
#include "layer.h"

#define BLOCK_STATUS_SEQ (3<<0)
#define BLOCK_STATUS_FREE (1<<2)
#define BLOCK_STATUS_NUM (1<<3)

struct flash_data {
	uint32_t block_status;
	uint16_t led_count;
	uint8_t led_mode;
	uint8_t background;
	struct layer layers[LAYER_MAX];
	uint32_t crc;
};

extern struct flash_data flash_data;

void ICACHE_FLASH_ATTR data_load(void);
void ICACHE_FLASH_ATTR data_save(void);
void ICACHE_FLASH_ATTR data_test(void);

#endif /* DATA_H */
