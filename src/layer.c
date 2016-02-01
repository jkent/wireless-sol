#include <esp8266.h>
#include "layer.h"
#include "led.h"
#include "data.h"
#include "util.h"

static void ICACHE_FLASH_ATTR apply_layer(struct layer *layer)
{
	/* validate ranges */
	for (uint8_t i = 0; i < RANGE_MAX; i++) {
		struct range *range = &layer->ranges[i];
		if (range->type == RANGE_TYPE_NONE) {
			break;
		}
		range->lb =	(range->lb < config_data.led_count) ?
				range->lb : config_data.led_count - 1;
		range->ub = (range->ub < config_data.led_count) ?
				range->ub : config_data.led_count - 1;
		if (range->lb > range->ub) {
			uint16_t tmp = range->lb;
			range->lb = range->ub;
			range->ub = tmp;
		}
	}

	/* apply RANGE_TYPE_SET/ADD/SUBTRACT */
	for (uint8_t i = 0; i < RANGE_MAX; i++) {
		struct range *range = &layer->ranges[i];
		if (range->type == RANGE_TYPE_NONE) {
			break;
		}
		if (range->type == RANGE_TYPE_SET) {
			uint8_t value = range->value & 0xFF;
			memset(led_next + range->lb, value, range->ub - range->lb + 1);
		} else if (range->type == RANGE_TYPE_ADD) {
			for (uint16_t j = range->lb; j <= range->ub; j++) {
				int value = led_next[j] + range->value;
				if (value > 255) {
					value = 255;
				}
				led_next[j] = value;
			}
		} else if (range->type == RANGE_TYPE_SUBTRACT) {
			for (uint16_t j = range->lb; j <= range->ub; j++) {
				int value = led_next[j] - range->value;
				if (value < 0) {
					value = 0;
				}
				led_next[j] = value;
			}
		}
	}

	/* apply RANGE_TYPE_COPY */
	for (uint8_t i = 0; i < RANGE_MAX; i++) {
		struct range *range = &layer->ranges[i];
		if (range->type == RANGE_TYPE_NONE) {
			break;
		}
		if (range->type != RANGE_TYPE_COPY) {
			continue;
		}
		range->value = (range->value < config_data.led_count) ?
				range->value : config_data.led_count - 1;
		memset(led_next + range->lb, led_next[range->value],
				range->ub - range->lb + 1);
	}

	/* apply RANGE_TYPE_TAPER */
	for (uint8_t i = 0; i < RANGE_MAX; i++) {
		struct range *range = &layer->ranges[i];
		if (range->type == RANGE_TYPE_NONE) {
			break;
		}
		if (range->type != RANGE_TYPE_TAPER) {
			continue;
		}
		uint8_t start = (range->lb - 1 < 0) ? 0 : led_next[range->lb - 1];
		uint8_t end = (range->ub + 1 >= config_data.led_count) ?
				0 : led_next[range->ub + 1];
		int16_t delta = (end - start) * 100 / (range->ub - range->lb + 2);
		uint16_t value = start * 100 + 50;
		for (uint16_t j = range->lb; j <= range->ub; j++) {
			value += delta;
			led_next[j] = value / 100;
		}
	}
}

void ICACHE_FLASH_ATTR layer_update(void)
{
	memset(led_next, status_data.background, config_data.led_count);

	for (uint8_t i = 0; i < LAYER_MAX; i++) {
		struct layer *layer = &config_data.layers[i];
		if (!layer->name[0]) {
			break;
		}
		if ((status_data.layer_state >> i & 1) == 0) {
			continue;
		}
		apply_layer(layer);
	}
}

int8_t ICACHE_FLASH_ATTR layer_find(const char *name)
{
	for (uint8_t i = 0; i < LAYER_MAX; i++) {
		struct layer *layer = &config_data.layers[i];
		if (!layer->name[0]) {
			break;
		}
		if (strncmp(layer->name, name, sizeof(layer->name)) == 0) {
			return i;
		}
	}
	return -1;
}

uint8_t ICACHE_FLASH_ATTR layer_count(void)
{
	for (uint8_t i = 0; i < LAYER_MAX; i++) {
		struct layer *layer = &config_data.layers[i];
		if (!layer->name[0]) {
			return i;
		}
	}
	return LAYER_MAX;
}

bool ICACHE_FLASH_ATTR layer_insert(uint8_t id, struct layer *layer)
{
	uint8_t count = layer_count();

	if (id > count || count >= LAYER_MAX || !layer) {
		return false;
	}

	if (count && id < count) {
		memmove(&config_data.layers[id + 1], &config_data.layers[id], sizeof(struct layer) * (count - id));
	}

	memcpy(&config_data.layers[id], layer, sizeof(struct layer));

	status_data.layer_state = bit_insert(id, status_data.layer_state);
	return true;
}

bool ICACHE_FLASH_ATTR layer_remove(uint8_t id, struct layer *layer)
{
	uint8_t count = layer_count();

	if (id >= count) {
		return false;
	}

	if (layer) {
		memcpy(layer, &config_data.layers[id], sizeof(struct layer));
	}

	if (id < count - 1) {
		memmove(&config_data.layers[id], &config_data.layers[id + 1], sizeof(struct layer) * (count - id - 1));
	}

	memset(&config_data.layers[count - 1], 0, sizeof(struct layer));

	status_data.layer_state = bit_remove(id, status_data.layer_state);
	return true;
}

bool ICACHE_FLASH_ATTR layer_move(uint8_t from, uint8_t to)
{
	uint8_t count = layer_count();
	struct layer layer;
	bool state;

	if (from >= count || to >= count) {
		return false;
	}

	if (from == to) {
		return true;
	}

	state = !!(status_data.layer_state & 1 << from);

	if (!layer_remove(from, &layer) || !layer_insert(to, &layer)) {
		return false;
	}

	status_data.layer_state |= state ? 1 << to : 0;
	return true;
}

uint8_t ICACHE_FLASH_ATTR range_count(struct layer *layer)
{
	uint8_t i;

	for (i = 0; i < RANGE_MAX; i++) {
		if (layer->ranges[i].type == RANGE_TYPE_NONE) {
			break;
		}
	}
	return i;
}

bool ICACHE_FLASH_ATTR range_add(struct layer *layer, struct range *range)
{
	uint8_t count, id;

	if (!layer || (count = range_count(layer)) >= RANGE_MAX) {
		return false;
	}

	if (range->type == RANGE_TYPE_NONE || range->type >= RANGE_TYPE_MAX) {
		return false;
	}

	if (range->lb > range->ub || range->ub >= config_data.led_count) {
		return false;
	}

	for (id = 0; id < count; id++) {
		if ((range->lb >= layer->ranges[id].lb && range->lb <= layer->ranges[id].ub) ||
			(range->ub >= layer->ranges[id].lb && range->ub <= layer->ranges[id].ub)) {
			return false;
		}
	}

	for (id = 0; id < count; id++) {
		if (range->lb < layer->ranges[id].lb) {
			break;
		}
	}

	if (count && id < count) {
		memmove(&layer->ranges[id + 1], &layer->ranges[id], sizeof(struct range) * (count - id));
	}

	memcpy(&layer->ranges[id], range, sizeof(struct range));
	return true;
}

bool ICACHE_FLASH_ATTR range_remove(struct layer *layer, uint8_t id, struct range *range)
{
	uint8_t count;

	if (!layer) {
		return false;
	}

	if (id >= (count = range_count(layer))) {
		return false;
	}

	if (range) {
		memcpy(range, &layer->ranges[id], sizeof(struct range));
	}

	if (id < count - 1) {
		memmove(&layer->ranges[id], &layer->ranges[id + 1], sizeof(struct range) * (count - id - 1));
	}

	memset(&layer->ranges[count - 1], 0, sizeof(struct range));
	return true;
}
