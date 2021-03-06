#include <esp8266.h>
#include "jsontree.h"
#include "json.h"
#include "data.h"
#include "led.h"
#include "layer.h"
#include "api.h"
#include "preset.h"

static int ICACHE_FLASH_ATTR settings_callback(struct jsontree_context *path)
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
		jsontree_write_int(path, config_data.led_count);
		break;
	case 2:
		jsontree_write_string(path, "fade_time");
		path->putchar(':');
		jsontree_write_int(path, config_data.fade_time);
		break;
	case 3:
		jsontree_write_string(path, "range_max");
		path->putchar(':');
		jsontree_write_int(path, RANGE_MAX);
		break;
	case 4:
		jsontree_write_string(path, "layer_max");
		path->putchar(':');
		jsontree_write_int(path, LAYER_MAX);
		break;
	case 5:
		jsontree_write_string(path, "layer_name_max");
		path->putchar(':');
		jsontree_write_int(path, LAYER_NAME_MAX);
		break;
	case 6:
		jsontree_write_string(path, "preset_max");
		path->putchar(':');
		jsontree_write_int(path, PRESET_MAX);
		break;
	case 7:
		jsontree_write_string(path, "preset_name_max");
		path->putchar(':');
		jsontree_write_int(path, PRESET_NAME_MAX);
		break;
	case 8:
		jsontree_write_string(path, "config_dirty");
		path->putchar(':');
		jsontree_write_atom(path, config_dirty ? "true" : "false");
		break;
	case 9:
		jsontree_write_string(path, "needs_layer_update");
		path->putchar(':');
		jsontree_write_atom(path, needs_layer_update ? "true" : "false");
		break;
	case 10:
		jsontree_write_string(path, "needs_led_update");
		path->putchar(':');
		jsontree_write_atom(path, needs_led_update ? "true" : "false");
		break;
	default:
		path->putchar('}');
		return 0;
	}

	if (path->callback_state < 11) {
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

	if (path->callback_state < config_data.led_count) {
		jsontree_write_int(path, led_current[path->callback_state++]);
	}
	else {
		path->putchar(']');
		return 0;
	}

	if (path->callback_state < config_data.led_count) {
		path->putchar(',');
	}

	return 1;
}

const struct jsontree_callback json_led_callback =
	JSONTREE_CALLBACK(led_callback, NULL);

static int ICACHE_FLASH_ATTR layer_list_callback(struct jsontree_context *path)
{
	struct layer *layer;
	char buf[LAYER_NAME_MAX + 1];

	if (path->callback_state == 0) {
		path->putchar('[');
	}

	if (path->callback_state >= LAYER_MAX) {
		path->putchar(']');
		return 0;
	}

	layer = &config_data.layers[path->callback_state];
	if (!layer->name[0]) {
		path->putchar(']');
		return 0;
	}

	path->putchar('{');

	jsontree_write_string(path, "name");
	path->putchar(':');
	strncpy(buf, layer->name, LAYER_NAME_MAX);
	buf[LAYER_NAME_MAX] = '\0';
	jsontree_write_string(path, buf);

	path->putchar(',');

	jsontree_write_string(path, "enabled");
	path->putchar(':');
	jsontree_write_atom(path, status_data.layers & (1 << path->callback_state) ? "true" : "false");

	path->putchar('}');

	if (path->callback_state < LAYER_MAX - 1) {
		layer++;
		if (layer->name[0]) {
			path->putchar(',');
		}
	}

	path->callback_state++;
	return 1;
}

static void ICACHE_FLASH_ATTR emit_range_object(struct jsontree_context *path, struct range *range)
{
	path->putchar('{');

	jsontree_write_string(path, "type");
	path->putchar(':');
	switch (range->type) {
	case RANGE_TYPE_SET:
		jsontree_write_string(path, "set");
		break;
	case RANGE_TYPE_ADD:
		jsontree_write_string(path, "add");
		break;
	case RANGE_TYPE_SUBTRACT:
		jsontree_write_string(path, "subtract");
		break;
	case RANGE_TYPE_COPY:
		jsontree_write_string(path, "copy");
		break;
	case RANGE_TYPE_TAPER:
		jsontree_write_string(path, "taper");
		break;
	default:
		jsontree_write_string(path, "unknown");
		break;
	}

	path->putchar(',');

	jsontree_write_string(path, "lb");
	path->putchar(':');
	jsontree_write_int(path, range->lb);

	path->putchar(',');

	jsontree_write_string(path, "ub");
	path->putchar(':');
	jsontree_write_int(path, range->ub);

	path->putchar(',');

	jsontree_write_string(path, "value");
	path->putchar(':');
	jsontree_write_int(path, range->value);

	path->putchar('}');
}

static int ICACHE_FLASH_ATTR layer_object_callback(struct jsontree_context *path, uint16_t id)
{
	uint8_t state = path->callback_state & 0xFF;
	uint8_t substate = (path->callback_state >> 8) & 0xFF;
	struct layer *layer = &config_data.layers[id];
	struct range *range;
	char buf[LAYER_NAME_MAX+1];

	if (path->callback_state == 0) {
		if (!layer->name[0]) {
			jsontree_write_atom(path, "null");
			return 0;
		}
		path->putchar('{');
	}

	switch (state) {
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
		jsontree_write_string(path, "enabled");
		path->putchar(':');
		jsontree_write_atom(path, status_data.layers & (1 << id) ? "true" : "false");
		path->putchar(',');
		path->callback_state++;
		break;
	case 2:
		if (substate == 0) {
			jsontree_write_string(path, "ranges");
			path->putchar(':');
			path->putchar('[');
		}

		if (substate >= RANGE_MAX) {
			path->putchar(']');
			path->callback_state++;
			break;
		}

		range = &layer->ranges[substate];
		if (range->type == RANGE_TYPE_NONE) {
			path->putchar(']');
			path->callback_state++;
			break;
		}

		emit_range_object(path, range);

		if (substate < RANGE_MAX - 1) {
			range++;
			if (range->type != RANGE_TYPE_NONE) {
				path->putchar(',');
			}
		}
		path->callback_state += 0x100;
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

static int ICACHE_FLASH_ATTR layer_background_callback(struct jsontree_context *path)
{
	jsontree_write_int(path, status_data.background);
	return 0;
}

const struct jsontree_callback json_layer_background_callback =
	JSONTREE_CALLBACK(layer_background_callback, NULL);


static int ICACHE_FLASH_ATTR preset_callback(struct jsontree_context *path)
{
	struct preset *preset;
	char buf[PRESET_NAME_MAX + 1];

	if (path->callback_state == 0) {
		path->putchar('[');
		jsontree_write_atom(path, "{\"name\":\"off\",\"background\":0,\"layers\":0}");
		if (config_data.preset_count > 0) {
			path->putchar(',');
		}
		path->callback_state++;
		return 1;
	}

	if (path->callback_state >= config_data.preset_count + 1) {
		path->putchar(']');
		return 0;
	}

	preset = &config_data.presets[path->callback_state - 1];

	path->putchar('{');

	jsontree_write_string(path, "name");
	path->putchar(':');
	strncpy(buf, preset->name, PRESET_NAME_MAX);
	buf[PRESET_NAME_MAX] = '\0';
	jsontree_write_string(path, buf);

	path->putchar(',');

	jsontree_write_string(path, "background");
	path->putchar(':');
	jsontree_write_int(path, preset->background);

	path->putchar(',');

	jsontree_write_string(path, "layers");
	path->putchar(':');
	jsontree_write_int(path, preset->layers);

	path->putchar('}');

	if (path->callback_state < config_data.preset_count) {
		path->putchar(',');
	}

	path->callback_state++;
	return 1;
}

const struct jsontree_callback json_preset_callback =
	JSONTREE_CALLBACK(preset_callback, NULL);
