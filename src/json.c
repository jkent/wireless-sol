#include <esp8266.h>
#include "json/jsontree.h"
#include "json.h"
#include "data.h"
#include "led.h"
#include "layer.h"

static int settings_callback(struct jsontree_context *path)
{
	if (path->callback_state == 0) {
		path->putchar('{');
	}

	switch (path->callback_state++) {
	case 0:
		jsontree_write_string(path, "led_max");
		path->putchar(':');
		jsontree_write_int(path, LED_MAX);
		break;
	case 1:
		jsontree_write_string(path, "led_count");
		path->putchar(':');
		jsontree_write_int(path, flash_data.led_count);
		break;
	case 2:
		jsontree_write_string(path, "range_max");
		path->putchar(':');
		jsontree_write_int(path, RANGE_MAX);
		break;
	case 3:
		jsontree_write_string(path, "layer_max");
		path->putchar(':');
		jsontree_write_int(path, LAYER_MAX);
		break;
	case 4:
		jsontree_write_string(path, "layer_name_max");
		path->putchar(':');
		jsontree_write_int(path, LAYER_NAME_MAX);
		break;
	default:
		path->putchar('}');
		return 0;
	}

	if (path->callback_state < 5) {
		path->putchar(',');
	}

	return 1;
}

const struct jsontree_callback json_settings_callback =
	JSONTREE_CALLBACK(settings_callback, NULL);

static int ICACHE_FLASH_ATTR led_callback(struct jsontree_context *path)
{
	if (path->callback_state == 0) {
		path->putchar('[');
	}

	if (path->callback_state < flash_data.led_count) {
		jsontree_write_int(path, led_current[path->callback_state++]);
	}
	else {
		path->putchar(']');
		return 0;
	}

	if (path->callback_state < flash_data.led_count) {
		path->putchar(',');
	}

	return 1;
}

const struct jsontree_callback json_led_callback =
	JSONTREE_CALLBACK(led_callback, NULL);

static int ICACHE_FLASH_ATTR layer_list_callback(struct jsontree_context *path)
{
	char buf[LAYER_NAME_MAX + 1];
	struct layer *layer;

	if (path->callback_state == 0) {
		path->putchar('{');
	}

	if (path->callback_state >= LAYER_MAX) {
		path->putchar('}');
		return 0;
	}

	layer = &flash_data.layers[path->callback_state];
	if (!layer->name[0]) {
		path->putchar('}');
		return 0;
	}

	strncpy(buf, layer->name, LAYER_NAME_MAX);
	buf[LAYER_NAME_MAX] = '\0';
	jsontree_write_string(path, buf);
	path->putchar(':');
	jsontree_write_atom(path, layer->visible ? "true" : "false");

	if (path->callback_state < LAYER_MAX - 1) {
		layer++;
		if (layer->name[0]) {
			path->putchar(',');
		}
	}

	path->callback_state++;
	return 1;
}

static int ICACHE_FLASH_ATTR range_object_callback(struct jsontree_context *path, struct range *range)
{
	uint16_t state = path->index[JSONTREE_MAX_DEPTH-3];

	if (state == 0) {
		path->putchar('{');
	}

	switch (state) {
	case 0:
		jsontree_write_string(path, "type");
		path->putchar(':');
		jsontree_write_int(path, range->type);
		break;
	case 1:
		jsontree_write_string(path, "lb");
		path->putchar(':');
		jsontree_write_int(path, range->lb);
		break;
	case 2:
		jsontree_write_string(path, "ub");
		path->putchar(':');
		jsontree_write_int(path, range->ub);
		break;
	case 3:
		jsontree_write_string(path, "value");
		path->putchar(':');
		jsontree_write_int(path, range->value);
		break;
	default:
		path->putchar('}');
		return 0;
	}

	if (state < 3) {
		path->putchar(',');
	}

	path->index[JSONTREE_MAX_DEPTH-3] = state + 1;
	return 1;
}

static int ICACHE_FLASH_ATTR layer_object_callback(struct jsontree_context *path, uint16_t id)
{
	struct layer *layer = &flash_data.layers[id];
	struct range *range;
	uint16_t range_id;
	char buf[LAYER_NAME_MAX+1];

	if (path->callback_state == 0) {
		path->index[JSONTREE_MAX_DEPTH-2] = 0;
		if (!layer->name[0]) {
			jsontree_write_atom(path, "null");
			return 0;
		}
		path->putchar('{');
	}

	switch (path->callback_state) {
	case 0:
		jsontree_write_string(path, "name");
		path->putchar(':');
		strncpy(buf, layer->name, LAYER_NAME_MAX);
		buf[LAYER_NAME_MAX] = '\0';
		jsontree_write_string(path, buf);
		path->putchar(',');
		path->callback_state++;
		break;
	case 1:
		jsontree_write_string(path, "ranges");
		path->putchar(':');
		path->putchar('[');
		path->index[JSONTREE_MAX_DEPTH-3] = 0;
		path->callback_state++;
		break;
	case 2:
		range_id = path->index[JSONTREE_MAX_DEPTH-2];

		if (range_id >= RANGE_MAX) {
			path->putchar(']');
			path->callback_state++;
			return 1;
		}

		range = &layer->ranges[range_id];
		if (range->type == RANGE_TYPE_NONE) {
			path->putchar(']');
			path->callback_state++;
			return 1;
		}

		if (!range_object_callback(path, range)) {
			path->index[JSONTREE_MAX_DEPTH-2] = range_id + 1;
			path->index[JSONTREE_MAX_DEPTH-3] = 0;
			if (range_id < RANGE_MAX - 1) {
				range++;
				if (range->type != RANGE_TYPE_NONE) {
					path->putchar(',');
				}
			}
		}
		break;
	default:
		path->putchar('}');
		return 0;
	}

	return 1;
}

static int ICACHE_FLASH_ATTR layer_callback(struct jsontree_context *path)
{
	uint16_t id = path->index[JSONTREE_MAX_DEPTH - 1];

	if (id == 65535) {
		return layer_list_callback(path);
	}

	if (id >= LAYER_MAX) {
		jsontree_write_atom(path, "null");
		return 0;
	}

	return layer_object_callback(path, id);
}

const struct jsontree_callback json_layer_callback =
	JSONTREE_CALLBACK(layer_callback, NULL);
