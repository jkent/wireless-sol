#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include "layer.h"

#define BLOCK_STATUS_SEQ (3<<0)
#define BLOCK_STATUS_FREE (1<<2)
#define BLOCK_STATUS_NUM (1<<3)

struct data_config {
	uint32_t block_status;

	/* configuration */
	uint16_t led_count;
	struct layer layers[LAYER_MAX];

	uint32_t crc;
};

struct data_status {
	uint8_t led_mode;
	uint8_t background;
	uint16_t layer_state;
};

extern struct data_config data_config;
extern struct data_status data_status;
extern bool data_unsaved_config;

void ICACHE_FLASH_ATTR data_init(void);
void ICACHE_FLASH_ATTR data_load(void);
void ICACHE_FLASH_ATTR data_save(void);
void ICACHE_FLASH_ATTR data_test(void);

#endif /* DATA_H */
