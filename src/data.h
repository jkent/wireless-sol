#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include "layer.h"

#define BLOCK_STATUS_SEQ (3<<0)
#define BLOCK_STATUS_FREE (1<<2)
#define BLOCK_STATUS_NUM (1<<3)

struct config_data {
	uint32_t block_status;

	uint16_t led_count;
	struct layer layers[LAYER_MAX];

	uint32_t crc;
};

struct status_data {
	uint32_t block_status;

	uint8_t led_mode;
	uint8_t background;
	uint16_t layer_state;

	uint32_t crc;
};

extern struct config_data config_data;
extern struct status_data status_data;
extern bool config_dirty;
extern bool status_dirty;

void ICACHE_FLASH_ATTR data_init(void);
void ICACHE_FLASH_ATTR config_load(void);
void ICACHE_FLASH_ATTR config_save(void);
void ICACHE_FLASH_ATTR status_load(void);
void ICACHE_FLASH_ATTR status_save(void);

#endif /* DATA_H */
