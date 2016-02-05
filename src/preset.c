#include <esp8266.h>
#include "data.h"
#include "led.h"
#include "layer.h"
#include "preset.h"

bool ICACHE_FLASH_ATTR preset_apply(uint8_t preset_num)
{
	if (preset_num > config_data.preset_count) {
		return false;
	}

	status_data.preset = preset_num;

	if (status_data.preset == 0) {
		status_data.background = 0;
		status_data.layers = 0;
	}
	else {
		struct preset *preset = &config_data.presets[preset_num - 1];
		status_data.background = preset->background;
		status_data.layers = preset->layers;
	}

	layer_update(true);
	return true;
}

void ICACHE_FLASH_ATTR preset_apply_next(void)
{
	uint8_t n = status_data.preset + 1;
	if (n > config_data.preset_count) {
		if (config_data.preset_count > 0) {
			n = 1;
		} else {
			n = 0;
		}
	}
	preset_apply(n);
}

int8_t ICACHE_FLASH_ATTR preset_find(const char *name)
{
	if (strcmp(name, "off") == 0) {
		return 0;
	}

	for (int i = 0; i < config_data.preset_count; i++) {
		struct preset *preset = &config_data.presets[i];
		if (strncmp(name, preset->name, PRESET_NAME_MAX) == 0) {
			return i + 1;
		}
	}

	return -1;
}

bool ICACHE_FLASH_ATTR preset_insert(uint8_t id, struct preset *preset)
{
	if (id == 0 || id - 1 > config_data.preset_count || config_data.preset_count >= PRESET_MAX || !preset) {
		return false;
	}

	if (config_data.preset_count && id <= config_data.preset_count) {
		memmove(&config_data.presets[id], &config_data.presets[id - 1], sizeof(struct preset) * (config_data.preset_count - (id - 1)));
	}

	memcpy(&config_data.presets[id - 1], preset, sizeof(struct preset));
	config_data.preset_count++;
	return true;
}

bool ICACHE_FLASH_ATTR preset_remove(uint8_t id, struct preset *preset)
{
	if (id == 0 || id > config_data.preset_count) {
		return false;
	}

	if (preset) {
		memcpy(preset, &config_data.presets[id - 1], sizeof(struct preset));
	}

	if (id < config_data.preset_count) {
		memmove(&config_data.presets[id - 1], &config_data.presets[id], sizeof(struct preset) * (config_data.preset_count - (id - 1) - 1));
	}

	config_data.preset_count--;
	return true;
}

bool ICACHE_FLASH_ATTR preset_move(uint8_t from, uint8_t to)
{
	struct preset preset;

	if (from == 0 || from - 1 >= config_data.preset_count || to == 0 || to - 1 >= config_data.preset_count) {
		return false;
	}

	if (from == to) {
		return true;
	}

	if (!preset_remove(from, &preset) || !preset_insert(to, &preset)) {
		return false;
	}

	return true;
}
