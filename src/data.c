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
bool data_unsaved_config;

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

	data_unsaved_config = false;

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
