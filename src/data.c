#include <esp8266.h>
#include "data.h"
#include "crc32.h"
#include "led.h"

#define FLASH_DATA_BLOCK0 0x3E000
#define FLASH_DATA_BLOCK1 0x3F000

#define FLASH_ERASE_SECTOR0 (FLASH_DATA_BLOCK0 >> 12)
#define FLASH_ERASE_SECTOR1 (FLASH_DATA_BLOCK1 >> 12)
#define FLASH_ERASE_COUNT (FLASH_ERASE_SECTOR1 - FLASH_ERASE_SECTOR0)

struct data_config data_config;
struct data_status data_status;

static bool ICACHE_FLASH_ATTR load_block(uint8_t block)
{
	if (block == 0) {
		spi_flash_read(FLASH_DATA_BLOCK0, (uint32 *)&data_config,
				sizeof(data_config));
	}
	else {
		spi_flash_read(FLASH_DATA_BLOCK1, (uint32 *)&data_config,
				sizeof(data_config));
	}

	if (crc32((uint8_t *)&data_config,
			sizeof(data_config) - sizeof(data_config.crc)) != data_config.crc) {
		return false;
	}

	data_config.block_status &= ~BLOCK_STATUS_NUM;
	if (block != 0) {
		data_config.block_status |= BLOCK_STATUS_NUM;
	}

	os_printf("data loaded, seq %d, block %d\n",
			data_config.block_status & BLOCK_STATUS_SEQ, block);

	return true;
}

void ICACHE_FLASH_ATTR data_init(void)
{
	data_load();
	memset(&data_status, 0, sizeof(data_status));
	data_config.led_count = 120;
}

void ICACHE_FLASH_ATTR data_load(void)
{
	uint32_t status[2];

	spi_flash_read(FLASH_DATA_BLOCK0, &status[0], sizeof(status[0]));
	spi_flash_read(FLASH_DATA_BLOCK1, &status[1], sizeof(status[1]));

	if ((status[0] & BLOCK_STATUS_FREE) && (status[1] & BLOCK_STATUS_FREE)) {
		goto init;
	}

	if (status[0] & BLOCK_STATUS_FREE) {
		if (load_block(1)) {
			return;
		}
		goto init;
	}

	if (status[1] & BLOCK_STATUS_FREE) {
		if (load_block(0)) {
			return;
		}
		goto init;
	}

	if ((status[1] & BLOCK_STATUS_SEQ) - (status[0] & BLOCK_STATUS_SEQ) == 1) {
		if (load_block(1) || load_block(0)) {
			return;
		}
	}
	else {
		if (load_block(0) || load_block(1)) {
			return;
		}
	}

init:
	memset(&data_config, 0, sizeof(data_config));
	data_config.block_status = 0xFFFFFFF0;
	os_printf("data initialized\n");
}

void ICACHE_FLASH_ATTR data_save(void)
{
	uint8_t seq = (data_config.block_status + 1) & BLOCK_STATUS_SEQ;
	uint8_t block = (data_config.block_status & BLOCK_STATUS_NUM) ? 0 : 1;

	data_unsaved_config = false;

	data_config.block_status = 0xFFFFFFF0 | (block ? BLOCK_STATUS_NUM : 0) | seq;
	data_config.crc = crc32((uint8_t *)&data_config,
			sizeof(data_config) - sizeof(data_config.crc));

	if (block == 0) {
		for (uint16 sector = FLASH_ERASE_SECTOR0;
				sector < FLASH_ERASE_SECTOR0 + FLASH_ERASE_COUNT; sector++) {
			spi_flash_erase_sector(sector);
		}
		spi_flash_write(FLASH_DATA_BLOCK0, (uint32 *)&data_config,
				sizeof(data_config));
	}
	else {
		for (uint16 sector = FLASH_ERASE_SECTOR1;
				sector < FLASH_ERASE_SECTOR1 + FLASH_ERASE_COUNT; sector++) {
			spi_flash_erase_sector(sector);
		}
		spi_flash_write(FLASH_DATA_BLOCK1, (uint32 *)&data_config,
				sizeof(data_config));
	}
}

void ICACHE_FLASH_ATTR data_test(void)
{
	memset(&flash_data, 0, sizeof(flash_data));

	/* configuration */
	flash_data.led_count = 120;

	struct layer *layer = flash_data.layers;
	strcpy(layer->name, "ends");
	layer->ranges[0].type = RANGE_TYPE_TAPER;
	layer->ranges[0].lb = 0;
	layer->ranges[0].ub = 14;
	layer->ranges[1].type = RANGE_TYPE_TAPER;
	layer->ranges[1].lb = 104;
	layer->ranges[1].ub = 119;

	layer++;
	strcpy(layer->name, "lcd");
	layer->ranges[0].type = RANGE_TYPE_SET;
	layer->ranges[0].lb = 0;
	layer->ranges[0].ub = 39;
	layer->ranges[0].value = 0;
	layer->ranges[1].type = RANGE_TYPE_TAPER;
	layer->ranges[1].lb = 40;
	layer->ranges[1].ub = 44;

	layer++;
	strcpy(layer->name, "taper");
	layer->ranges[0].type = RANGE_TYPE_SET;
	layer->ranges[0].lb = 0;
	layer->ranges[0].ub = 0;
	layer->ranges[0].value = 0;
	layer->ranges[1].type = RANGE_TYPE_TAPER;
	layer->ranges[1].lb = 1;
	layer->ranges[1].ub = 118;
	layer->ranges[2].type = RANGE_TYPE_SET;
	layer->ranges[2].lb = 119;
	layer->ranges[2].ub = 119;
	layer->ranges[2].value = 255;

	/* status */
	flash_data.led_mode = LED_MODE_LAYER;
	flash_data.background = 255;
	flash_data.layer_state = 0x0003;
}
