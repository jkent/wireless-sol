#include <esp8266.h>
#include "data.h"
#include "crc32.h"
#include "led.h"

#define CONFIG0_BLOCK_ADDR 0x3C000
#define CONFIG1_BLOCK_ADDR 0x3D000
#define CONFIG0_SECTOR (CONFIG0_BLOCK_ADDR >> 12)
#define CONFIG1_SECTOR (CONFIG1_BLOCK_ADDR >> 12)
#define CONFIG_ERASE_COUNT (CONFIG1_SECTOR - CONFIG0_SECTOR)

#define STATUS0_BLOCK_ADDR 0x3E000
#define STATUS1_BLOCK_ADDR 0x3F000
#define STATUS0_SECTOR (STATUS0_BLOCK_ADDR >> 12)
#define STATUS1_SECTOR (STATUS1_BLOCK_ADDR >> 12)
#define STATUS_ERASE_COUNT (STATUS1_SECTOR - STATUS0_SECTOR)

struct config_data config_data;
struct status_data status_data;
bool config_dirty;
bool status_dirty;

static bool ICACHE_FLASH_ATTR load_config(uint8_t block)
{
	if (block == 0) {
		spi_flash_read(CONFIG0_BLOCK_ADDR, (uint32 *)&config_data,
				sizeof(config_data));
	}
	else {
		spi_flash_read(CONFIG1_BLOCK_ADDR, (uint32 *)&config_data,
				sizeof(config_data));
	}

	if (crc32((uint8_t *)&config_data,
			sizeof(config_data) - sizeof(config_data.crc)) != config_data.crc) {
		return false;
	}

	config_data.block_status &= ~BLOCK_STATUS_NUM;
	if (block != 0) {
		config_data.block_status |= BLOCK_STATUS_NUM;
	}

	os_printf("config loaded, seq %d, block %d\n",
			config_data.block_status & BLOCK_STATUS_SEQ, block);

	return true;
}

static bool ICACHE_FLASH_ATTR load_status(uint8_t block)
{
	if (block == 0) {
		spi_flash_read(STATUS0_BLOCK_ADDR, (uint32 *)&status_data,
				sizeof(status_data));
	}
	else {
		spi_flash_read(STATUS1_BLOCK_ADDR, (uint32 *)&status_data,
				sizeof(status_data));
	}

	if (crc32((uint8_t *)&status_data,
			sizeof(status_data) - sizeof(status_data.crc)) != status_data.crc) {
		return false;
	}

	status_data.block_status &= ~BLOCK_STATUS_NUM;
	if (block != 0) {
		status_data.block_status |= BLOCK_STATUS_NUM;
	}

	os_printf("status loaded, seq %d, block %d\n",
			status_data.block_status & BLOCK_STATUS_SEQ, block);

	return true;
}

void ICACHE_FLASH_ATTR data_init(void)
{
	config_load();
	status_load();
}

void ICACHE_FLASH_ATTR config_load(void)
{
	uint32_t status[2];

	config_dirty = false;

	spi_flash_read(CONFIG0_BLOCK_ADDR, &status[0], sizeof(status[0]));
	spi_flash_read(CONFIG1_BLOCK_ADDR, &status[1], sizeof(status[1]));

	if ((status[0] & BLOCK_STATUS_FREE) && (status[1] & BLOCK_STATUS_FREE)) {
		goto init;
	}

	if (status[0] & BLOCK_STATUS_FREE) {
		if (load_config(1)) {
			return;
		}
		goto init;
	}

	if (status[1] & BLOCK_STATUS_FREE) {
		if (load_config(0)) {
			return;
		}
		goto init;
	}

	if ((status[1] & BLOCK_STATUS_SEQ) - (status[0] & BLOCK_STATUS_SEQ) == 1) {
		if (load_config(1) || load_config(0)) {
			return;
		}
	}
	else {
		if (load_config(0) || load_config(1)) {
			return;
		}
	}

init:
	memset(&config_data, 0, sizeof(config_data));
	config_data.block_status = 0xFFFFFFF0;
	os_printf("config initialized\n");
}

void ICACHE_FLASH_ATTR config_save(void)
{
	uint8_t seq = (config_data.block_status + 1) & BLOCK_STATUS_SEQ;
	uint8_t block = (config_data.block_status & BLOCK_STATUS_NUM) ? 0 : 1;

	config_dirty = false;

	config_data.block_status = 0xFFFFFFF0 | (block ? BLOCK_STATUS_NUM : 0) | seq;
	config_data.crc = crc32((uint8_t *)&config_data,
			sizeof(config_data) - sizeof(config_data.crc));

	if (block == 0) {
		for (uint16 sector = CONFIG0_SECTOR;
				sector < CONFIG0_SECTOR + CONFIG_ERASE_COUNT; sector++) {
			spi_flash_erase_sector(sector);
		}
		spi_flash_write(CONFIG0_BLOCK_ADDR, (uint32 *)&config_data,
				sizeof(config_data));
	}
	else {
		for (uint16 sector = CONFIG1_SECTOR;
				sector < CONFIG1_SECTOR + CONFIG_ERASE_COUNT; sector++) {
			spi_flash_erase_sector(sector);
		}
		spi_flash_write(CONFIG1_BLOCK_ADDR, (uint32 *)&config_data,
				sizeof(config_data));
	}
}

void ICACHE_FLASH_ATTR status_load(void)
{
	uint32_t status[2];

	status_dirty = false;

	spi_flash_read(STATUS0_BLOCK_ADDR, &status[0], sizeof(status[0]));
	spi_flash_read(STATUS1_BLOCK_ADDR, &status[1], sizeof(status[1]));

	if ((status[0] & BLOCK_STATUS_FREE) && (status[1] & BLOCK_STATUS_FREE)) {
		goto init;
	}

	if (status[0] & BLOCK_STATUS_FREE) {
		if (load_status(1)) {
			return;
		}
		goto init;
	}

	if (status[1] & BLOCK_STATUS_FREE) {
		if (load_status(0)) {
			return;
		}
		goto init;
	}

	if ((status[1] & BLOCK_STATUS_SEQ) - (status[0] & BLOCK_STATUS_SEQ) == 1) {
		if (load_status(1) || load_status(0)) {
			return;
		}
	}
	else {
		if (load_status(0) || load_status(1)) {
			return;
		}
	}

init:
	memset(&status_data, 0, sizeof(status_data));
	status_data.block_status = 0xFFFFFFF0;
	os_printf("status initialized\n");
}

void ICACHE_FLASH_ATTR status_save(void)
{
	uint8_t seq = (status_data.block_status + 1) & BLOCK_STATUS_SEQ;
	uint8_t block = (status_data.block_status & BLOCK_STATUS_NUM) ? 0 : 1;

	status_dirty = false;

	status_data.block_status = 0xFFFFFFF0 | (block ? BLOCK_STATUS_NUM : 0) | seq;
	status_data.crc = crc32((uint8_t *)&status_data,
			sizeof(status_data) - sizeof(status_data.crc));

	if (block == 0) {
		for (uint16 sector = STATUS0_SECTOR;
				sector < STATUS0_SECTOR + STATUS_ERASE_COUNT; sector++) {
			spi_flash_erase_sector(sector);
		}
		spi_flash_write(STATUS0_BLOCK_ADDR, (uint32 *)&status_data,
				sizeof(status_data));
	}
	else {
		for (uint16 sector = STATUS1_SECTOR;
				sector < STATUS1_SECTOR + STATUS_ERASE_COUNT; sector++) {
			spi_flash_erase_sector(sector);
		}
		spi_flash_write(STATUS1_BLOCK_ADDR, (uint32 *)&status_data,
				sizeof(status_data));
	}
}
