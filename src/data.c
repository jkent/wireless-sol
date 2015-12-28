#include <esp8266.h>
#include "data.h"
#include "crc32.h"

#define FLASH_DATA_BLOCK0 0x3E000
#define FLASH_DATA_BLOCK1 0x3F000

#define FLASH_ERASE_SECTOR0 (FLASH_DATA_BLOCK0 >> 12)
#define FLASH_ERASE_SECTOR1 (FLASH_DATA_BLOCK1 >> 12)
#define FLASH_ERASE_COUNT (FLASH_ERASE_SECTOR1 - FLASH_ERASE_SECTOR0)

struct flash_data flash_data;

static bool ICACHE_FLASH_ATTR load_block(uint8_t block)
{
	if (block == 0) {
		spi_flash_read(FLASH_DATA_BLOCK0, (uint32 *)&flash_data,
				sizeof(flash_data));
	}
	else {
		spi_flash_read(FLASH_DATA_BLOCK1, (uint32 *)&flash_data,
				sizeof(flash_data));
	}

	if (crc32((uint8_t *)&flash_data,
			sizeof(flash_data) - sizeof(flash_data.crc)) != flash_data.crc) {
		return false;
	}

	flash_data.block_status &= ~BLOCK_STATUS_NUM;
	if (block != 0) {
		flash_data.block_status |= BLOCK_STATUS_NUM;
	}

	os_printf("data loaded, seq %d, block %d\n", flash_data.block_status & BLOCK_STATUS_SEQ, block);

	return true;
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
	memset(&flash_data, 0, sizeof(flash_data));
	flash_data.block_status = 0xFFFFFFF0;
	os_printf("data initialized\n");
}

void ICACHE_FLASH_ATTR data_save(void)
{
	uint8_t seq = (flash_data.block_status + 1) & BLOCK_STATUS_SEQ;
	uint8_t block = (flash_data.block_status & BLOCK_STATUS_NUM) ? 0 : 1;

	flash_data.block_status = 0xFFFFFFF0 | (block ? BLOCK_STATUS_NUM : 0) | seq;
	flash_data.crc = crc32((uint8_t *)&flash_data,
			sizeof(flash_data) - sizeof(flash_data.crc));

	if (block == 0) {
		for (uint16 sector = FLASH_ERASE_SECTOR0;
				sector < FLASH_ERASE_SECTOR0 + FLASH_ERASE_COUNT; sector++) {
			spi_flash_erase_sector(sector);
		}
		spi_flash_write(FLASH_DATA_BLOCK0, (uint32 *)&flash_data,
				sizeof(flash_data));
	}
	else {
		for (uint16 sector = FLASH_ERASE_SECTOR1;
				sector < FLASH_ERASE_SECTOR1 + FLASH_ERASE_COUNT; sector++) {
			spi_flash_erase_sector(sector);
		}
		spi_flash_write(FLASH_DATA_BLOCK1, (uint32 *)&flash_data,
				sizeof(flash_data));
	}
}

void ICACHE_FLASH_ATTR data_test(void)
{
	memset(&flash_data, 0, sizeof(flash_data));

	flash_data.led_count = 120;
	flash_data.led_mode = LED_MODE_LAYER;

	flash_data.background = 255;

	struct layer *layer = flash_data.layers;
	strcpy(layer->name, "ends");
	layer->visible = true;
	layer->ranges[0].type = RANGE_TYPE_TAPER;
	layer->ranges[0].lb = 0;
	layer->ranges[0].ub = 14;
	layer->ranges[1].type = RANGE_TYPE_TAPER;
	layer->ranges[1].lb = 104;
	layer->ranges[1].ub = 119;

	layer++;
	strcpy(layer->name, "lcd");
	layer->visible = true;
	layer->ranges[0].type = RANGE_TYPE_SET;
	layer->ranges[0].lb = 0;
	layer->ranges[0].ub = 39;
	layer->ranges[0].value = 0;
	layer->ranges[1].type = RANGE_TYPE_TAPER;
	layer->ranges[1].lb = 40;
	layer->ranges[1].ub = 44;

	layer++;
	strcpy(layer->name, "taper");
	layer->visible = false;
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
}
