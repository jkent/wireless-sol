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
		range->lb =	(range->lb < flash_data.led_count) ?
				range->lb : flash_data.led_count - 1;
		range->ub = (range->ub < flash_data.led_count) ?
				range->ub : flash_data.led_count - 1;
		if (range->lb > range->ub) {
			uint16_t tmp = range->lb;
			range->lb = range->ub;
			range->ub = tmp;
		}
	}

	/* apply RANGE_TYPE_SET */
	for (uint8_t i = 0; i < RANGE_MAX; i++) {
		struct range *range = &layer->ranges[i];
		if (range->type == RANGE_TYPE_NONE) {
			break;
		}
		if (range->type != RANGE_TYPE_SET) {
			continue;
		}
		uint8_t value = range->value & 0xFF;
		memset(led_next + range->lb, value, range->ub - range->lb + 1);
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
		range->value = (range->value < flash_data.led_count) ?
				range->value : flash_data.led_count - 1;
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
		uint8_t end = (range->ub + 1 >= flash_data.led_count) ?
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
	memset(led_next, flash_data.background, flash_data.led_count);

	for (uint8_t i = 0; i < LAYER_MAX; i++) {
		struct layer *layer = &flash_data.layers[i];
		if (!layer->name[0]) {
			break;
		}
		if ((flash_data.layer_state >> i & 1) == 0) {
			continue;
		}
		apply_layer(layer);
	}
}

int8_t ICACHE_FLASH_ATTR layer_find(const char *name)
{
	for (uint8_t i = 0; i < LAYER_MAX; i++) {
		struct layer *layer = &flash_data.layers[i];
		if (!layer->name[0]) {
			break;
		}
		if (strcmp(layer->name, name) == 0) {
			return i;
		}
	}
	return -1;
}

static uint8_t ICACHE_FLASH_ATTR layer_count(void)
{
	for (uint8_t i = 0; i < LAYER_MAX; i++) {
		struct layer *layer = &flash_data.layers[i];
		if (!layer->name[0]) {
			return i;
		}
	}
	return LAYER_MAX;
}

bool ICACHE_FLASH_ATTR layer_remove(uint8_t id, struct layer *layer)
{
	uint8_t count = layer_count();

	if (id >= count) {
		return false;
	}

	if (layer) {
		memcpy(layer, &flash_data.layers[id], sizeof(struct layer));
	}

	if (id < count - 1) {
		memmove(&flash_data.layers[id], &flash_data.layers[id + 1], sizeof(struct layer) * (count - id - 1));
	}

	memset(&flash_data.layers[count - 1], 0, sizeof(struct layer));

	flash_data.layer_state = bit_remove(id, flash_data.layer_state);
	return true;
}

bool ICACHE_FLASH_ATTR layer_insert(uint8_t id, struct layer *layer)
{
	uint8_t count = layer_count();

	if (id > count || count >= LAYER_MAX || !layer) {
		return false;
	}

	if (count && id < count) {
		memmove(&flash_data.layers[id + 1], &flash_data.layers[id], sizeof(struct layer) * (count - id));
	}

	memcpy(&flash_data.layers[id], layer, sizeof(struct layer));

	flash_data.layer_state = bit_insert(id, flash_data.layer_state);
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

	state = !!(flash_data.layer_state & 1 << from);

	if (!layer_remove(from, &layer) || !layer_insert(to, &layer)) {
		return false;
	}

	flash_data.layer_state |= state ? 1 << to : 0;
	return true;
}
