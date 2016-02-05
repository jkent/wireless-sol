#include <esp8266.h>
#include "api.h"
#include "httpd.h"
#include "data.h"
#include "led.h"
#include "preset.h"

bool needs_layer_update = false;
bool needs_led_update = false;

static int ICACHE_FLASH_ATTR config_save_handler(struct jsonparse_state *state, const char *action)
{
	if (strcmp(action, "load") == 0) {
		config_load();
	} else if (strcmp(action, "save") == 0) {
		config_save();
	} else {
		return API_FAIL;
	}
	return API_OK;
}

static int ICACHE_FLASH_ATTR layer_enable_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	int layer_id = -1;
	char type;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "layer") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				layer_id = layer_find(name);
			} else if (type == '0') {
				layer_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (layer_id < 0) {
		return API_ERROR_PARSE;
	}

	if (layer_id >= layer_count()) {
		return API_FAIL;
	}

	if (strcmp(action, "disable") == 0) {
		status_data.layers &= ~(1 << layer_id);
	} else if (strcmp(action, "enable") == 0) {
		status_data.layers |= 1 << layer_id;
	}

	needs_layer_update = true;
	status_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR layer_insert_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	struct layer layer;
	int insert_at = -1;
	char type;

	memset(&layer, 0, sizeof(layer));

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "name") == 0 && type == '"') {
			jsonparse_copy_value(state, name, sizeof(name));
			strncpy(layer.name, name, sizeof(layer.name));
		} else if (strcmp(name, "at") == 0 && type == '0') {
			insert_at = jsonparse_get_value_as_int(state);
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (layer.name[0] == '\0') {
		return API_ERROR_PARSE;
	}

	if (layer_find(layer.name) >= 0) {
		return API_FAIL;
	}

	if (insert_at < 0) {
		insert_at = layer_count();
	}

	if (!layer_insert(insert_at, &layer)) {
		return API_FAIL;
	}

	config_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR layer_move_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	int layer_id, move_to;
	char type;

	layer_id = move_to = -1;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "layer") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				layer_id = layer_find(name);
			} else if (type == '0') {
				layer_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else if (strcmp(name, "to") == 0 && type == '0') {
			move_to = jsonparse_get_value_as_int(state);
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (!layer_move(layer_id, move_to)) {
		return API_FAIL;
	}

	if (status_data.layers & 1 << move_to) {
		needs_layer_update = true;
	}

	config_dirty = true;
	status_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR layer_remove_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	int layer_id = -1;
	char type;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "layer") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				layer_id = layer_find(name);
			} else if (type == '0') {
				layer_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else {
			return API_ERROR_PARSE;
		}
	}

	bool was_enabled = !!(status_data.layers & 1 << layer_id);

	if (!layer_remove(layer_id, NULL)) {
		return API_FAIL;
	}

	if (was_enabled) {
		needs_layer_update = true;
	}

    config_dirty = true;
	status_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR layer_rename_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	char new[LAYER_NAME_MAX + 1];
	int layer_id = -1;
	char type;
	struct layer *layer;

	new[0] = '\0';

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "layer") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				layer_id = layer_find(name);
			} else if (type == '0') {
				layer_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else if (strcmp(name, "name") == 0 && type == '"') {
			jsonparse_copy_value(state, new, sizeof(new));
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (layer_id < 0 || new[0] == '\0') {
		return API_ERROR_PARSE;
	}

	if (layer_id >= layer_count()) {
		return API_FAIL;
	}

	layer = &config_data.layers[layer_id];

	if (strcmp(layer->name, new) == 0) {
		return API_OK;
	}

	if (layer_find(new) >= 0) {
		return API_FAIL;
	}

	strncpy(layer->name, new, sizeof(layer->name));

	config_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR layer_update_handler(struct jsonparse_state *state, const char *action)
{
	layer_update(true);
	return API_OK;
}

static int ICACHE_FLASH_ATTR layer_background_set_handler(struct jsonparse_state *state, const char *action)
{
	char name[6];
	int value;
	char type;

	value = status_data.background;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "value") == 0 && type == '0') {
			value = jsonparse_get_value_as_int(state);
			if (value < 0 || value > 255) {
				return API_ERROR_PARSE;
			}
		} else {
			return API_ERROR_PARSE;
		}
	}

	status_data.background = value;

	needs_layer_update = true;
	status_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR led_set_handler(struct jsonparse_state *state, const char *action)
{
	char name[7];
	uint16_t i;
	int value;
	uint8_t values[LED_MAX];
	char type;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "values") == 0 && type == '[') {
			i = 0;
			while (true) {
				if ((type = jsonparse_next(state)) == ']') {
					break;
				} else if (type == ',') {
					continue;
				} else if (type != '0') {
					return API_ERROR_PARSE;
				}

				if (i >= config_data.led_count) {
					return API_FAIL;
				}

				value = jsonparse_get_value_as_int(state);
				if (value < 0 || value > 255) {
					return API_ERROR_PARSE;
				}

				values[i++] = value;
			}

			if (i < config_data.led_count) {
				return API_FAIL;
			}
		} else {
			return API_ERROR_PARSE;
		}
	}

	memcpy(led_current, values, config_data.led_count);

	needs_led_update = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR led_update_handler(struct jsonparse_state *state, const char *action)
{
	led_update();
	return API_OK;
}

static int ICACHE_FLASH_ATTR preset_apply_handler(struct jsonparse_state *state, const char *action)
{
	char name[PRESET_NAME_MAX + 1];
	int preset_id;
	char type;

	preset_id = -1;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "preset") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				preset_id = preset_find(name);
			} else if (type == '0') {
				preset_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (!preset_apply(preset_id)) {
		return API_FAIL;
	}

	return API_OK;
}

static int ICACHE_FLASH_ATTR preset_edit_handler(struct jsonparse_state *state, const char *action)
{
	char name[PRESET_NAME_MAX + 1];
	struct preset *preset;
	int preset_id = -1;
	bool have_background, have_layers;
	char type;
	uint8_t background;
	uint16_t layers;

	have_background = have_layers = false;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "preset") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				preset_id = preset_find(name);
			} else if (type == '0') {
				preset_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else if (strcmp(name, "background") == 0 && type == '0') {
			background = jsonparse_get_value_as_int(state);
			have_background = true;
		} else if (strcmp(name, "layers") == 0 && type == '0') {
			layers = jsonparse_get_value_as_int(state);
			have_layers = true;
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (preset_id < 1) {
		return API_ERROR_PARSE;
	}

	if (preset_id > config_data.preset_count) {
		return API_FAIL;
	}

	preset = &config_data.presets[preset_id - 1];

	if (have_background) {
		preset->background = background;
	}

	if (have_layers) {
		preset->layers = layers;
	}

	config_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR preset_insert_handler(struct jsonparse_state *state, const char *action)
{
	char name[PRESET_NAME_MAX + 1];
	struct preset preset;
	int insert_at = -1;
	char type;

	memset(&preset, 0, sizeof(struct preset));

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "name") == 0 && type == '"') {
			jsonparse_copy_value(state, name, sizeof(name));
			strncpy(preset.name, name, sizeof(preset.name));
		} else if (strcmp(name, "at") == 0 && type == '0') {
			insert_at = jsonparse_get_value_as_int(state);
		} else if (strcmp(name, "background") == 0 && type == '0') {
			preset.background = jsonparse_get_value_as_int(state);
		} else if (strcmp(name, "layers") == 0 && type == '0') {
			preset.layers = jsonparse_get_value_as_int(state);
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (preset.name[0] == '\0') {
		return API_ERROR_PARSE;
	}

	if (preset_find(preset.name) >= 0) {
		return API_FAIL;
	}

	if (insert_at < 0) {
		insert_at = config_data.preset_count + 1;
	}

	if (!preset_insert(insert_at, &preset)) {
		return API_FAIL;
	}

	config_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR preset_move_handler(struct jsonparse_state *state, const char *action)
{
	char name[PRESET_NAME_MAX + 1];
	int preset_id, move_to;
	char type;

	preset_id = move_to = -1;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "preset") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				preset_id = preset_find(name);
			} else if (type == '0') {
				preset_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else if (strcmp(name, "to") == 0 && type == '0') {
			move_to = jsonparse_get_value_as_int(state);
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (!preset_move(preset_id, move_to)) {
		return API_FAIL;
	}

	config_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR preset_remove_handler(struct jsonparse_state *state, const char *action)
{
	char name[PRESET_NAME_MAX + 1];
	int preset_id = -1;
	char type;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "preset") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				preset_id = preset_find(name);
			} else if (type == '0') {
				preset_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (!preset_remove(preset_id, NULL)) {
		return API_FAIL;
	}

	config_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR preset_rename_handler(struct jsonparse_state *state, const char *action)
{
	char name[PRESET_NAME_MAX + 1];
	char new[PRESET_NAME_MAX + 1];
	int preset_id = -1;
	char type;
	struct preset *preset;

	new[0] = '\0';

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "preset") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				preset_id = preset_find(name);
			} else if (type == '0') {
				preset_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else if (strcmp(name, "name") == 0 && type == '"') {
			jsonparse_copy_value(state, new, sizeof(new));
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (preset_id < 1 || new[0] == '\0') {
		return API_ERROR_PARSE;
	}

	if (preset_id > config_data.preset_count) {
		return API_FAIL;
	}

	preset = &config_data.presets[preset_id - 1];

	if (strcmp(preset->name, new) == 0) {
		return API_OK;
	}

	if (preset_find(new) >= 0) {
		return API_FAIL;
	}

	strncpy(preset->name, new, sizeof(preset->name));
	config_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR range_add_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	struct layer *layer;
	struct range range;
	uint8_t layer_id = -1;
	bool have_type, have_lb, have_ub, have_value;
	char type;

	have_type = have_lb = have_ub = have_value = false;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "layer") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				layer_id = layer_find(name);
			} else if (type == '0') {
				layer_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else if (strcmp(name, "type") == 0 && type == '"') {
			if (jsonparse_strcmp_value(state, "set") == 0) {
				range.type = RANGE_TYPE_SET;
			} else if (jsonparse_strcmp_value(state, "add") == 0) {
				range.type = RANGE_TYPE_ADD;
			} else if (jsonparse_strcmp_value(state, "subtract") == 0) {
				range.type = RANGE_TYPE_SUBTRACT;
			} else if (jsonparse_strcmp_value(state, "copy") == 0) {
				range.type = RANGE_TYPE_COPY;
			} else if (jsonparse_strcmp_value(state, "taper") == 0) {
				range.type = RANGE_TYPE_TAPER;
			} else {
				return API_ERROR_PARSE;
			}
			have_type = true;
		} else if (strcmp(name, "lb") == 0 && type == '0') {
			range.lb = jsonparse_get_value_as_int(state);
			have_lb = true;
		} else if (strcmp(name, "ub") == 0 && type == '0') {
			range.ub = jsonparse_get_value_as_int(state);
			have_ub = true;
		} else if (strcmp(name, "value") == 0 && type == '0') {
			range.value = jsonparse_get_value_as_int(state);
			have_value = true;
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (layer_id < 0) {
		return API_ERROR_PARSE;
	}

	if (layer_id >= layer_count()) {
		return API_FAIL;
	}

	layer = &config_data.layers[layer_id];

	if (!have_type || !have_lb || !have_ub ) {
		return API_ERROR_PARSE;
	}

	if (!have_value) {
		range.value = 0;
	}

	if (!range_add(layer, &range)) {
		return API_FAIL;
	}

	if (status_data.layers & 1 << layer_id) {
		needs_layer_update = true;
	}

	config_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR range_edit_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	struct layer *layer;
	struct range range;
	uint8_t layer_id = -1;
	uint8_t range_id = -1;
	uint8_t count;
	bool have_type, have_lb, have_ub, have_value;
	char type;

	have_type = have_lb = have_ub = have_value = false;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "layer") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				layer_id = layer_find(name);
			} else if (type == '0') {
				layer_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else if (strcmp(name, "range") == 0 && type == '0') {
			range_id = jsonparse_get_value_as_int(state);
		} else if (strcmp(name, "type") == 0 && type == '"') {
			if (jsonparse_strcmp_value(state, "set") == 0) {
				range.type = RANGE_TYPE_SET;
			} else if (jsonparse_strcmp_value(state, "add") == 0) {
				range.type = RANGE_TYPE_ADD;
			} else if (jsonparse_strcmp_value(state, "subtract") == 0) {
				range.type = RANGE_TYPE_SUBTRACT;
			} else if (jsonparse_strcmp_value(state, "copy") == 0) {
				range.type = RANGE_TYPE_COPY;
			} else if (jsonparse_strcmp_value(state, "taper") == 0) {
				range.type = RANGE_TYPE_TAPER;
			} else {
				return API_ERROR_PARSE;
			}
			have_type = true;
		} else if (strcmp(name, "lb") == 0 && type == '0') {
			range.lb = jsonparse_get_value_as_int(state);
			have_lb = true;
		} else if (strcmp(name, "ub") == 0 && type == '0') {
			range.ub = jsonparse_get_value_as_int(state);
			have_ub = true;
		} else if (strcmp(name, "value") == 0 && type == '0') {
			range.value = jsonparse_get_value_as_int(state);
			have_value = true;
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (layer_id < 0) {
		return API_ERROR_PARSE;
	}

	if (layer_id >= layer_count()) {
		return API_FAIL;
	}

	layer = &config_data.layers[layer_id];

	count = range_count(layer);

	if (range_id < 0) {
		return API_ERROR_PARSE;
	}

	if (range_id >= count) {
		return API_FAIL;
	}

	if (!have_type) {
		range.type = layer->ranges[range_id].type;
	}

	if (!have_lb) {
		range.lb = layer->ranges[range_id].lb;
	}

	if (!have_ub) {
		range.ub = layer->ranges[range_id].ub;
	}

	if (!have_value) {
		range.value = layer->ranges[range_id].value;
	}

	if (range.lb > range.ub || range.ub >= config_data.led_count) {
		return API_FAIL;
	}

	if (range_id > 0 && range.lb <= layer->ranges[range_id - 1].ub) {
		return API_FAIL;
	}

	if (count && range_id < count - 1 && range.ub >= layer->ranges[range_id + 1].lb) {
		return API_FAIL;
	}

	memcpy(&layer->ranges[range_id], &range, sizeof(struct range));

	if (status_data.layers & 1 << layer_id) {
		needs_layer_update = true;
	}

	config_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR range_remove_handler(struct jsonparse_state *state, const char *action)
{
	char name[LAYER_NAME_MAX + 1];
	struct layer *layer;
	uint8_t layer_id = -1;
	uint8_t range_id = -1;
	char type;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "layer") == 0) {
			if (type == '"') {
				jsonparse_copy_value(state, name, sizeof(name));
				layer_id = layer_find(name);
			} else if (type == '0') {
				layer_id = jsonparse_get_value_as_int(state);
			} else {
				return API_ERROR_PARSE;
			}
		} else if (strcmp(name, "range") == 0 && type == '0') {
			range_id = jsonparse_get_value_as_int(state);
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (layer_id < 0) {
		return API_ERROR_PARSE;
	}

	if (layer_id >= layer_count()) {
		return API_FAIL;
	}

	layer = &config_data.layers[layer_id];

	if (!range_remove(layer, range_id, NULL)) {
		return API_FAIL;
	}

	if (status_data.layers & 1 << layer_id) {
		needs_layer_update = true;
	}

	config_dirty = true;
	return API_OK;
}

static int ICACHE_FLASH_ATTR settings_set_handler(struct jsonparse_state *state, const char *action)
{
	char name[10];
	uint16_t led_count;
	char type;

	led_count = config_data.led_count;

	if (jsonparse_next(state) != '{') {
		return API_ERROR_PARSE;
	}

	while (true) {
		if ((type = jsonparse_next(state)) == '}') {
			break;
		} else if (type == ',') {
			continue;
		} else if (type != 'N') {
			return API_ERROR_PARSE;
		}

		jsonparse_copy_value(state, name, sizeof(name));
		type = jsonparse_next(state);
		if (strcmp(name, "led_count") == 0 && type == '0') {
			led_count = jsonparse_get_value_as_int(state);
		} else {
			return API_ERROR_PARSE;
		}
	}

	if (led_count > LED_MAX) {
		return API_FAIL;
	}

	config_data.led_count = led_count;

	needs_layer_update = true;
	config_dirty = true;
	return API_OK;
}

static struct api_handler handlers[] = {
	{"config", "load", config_save_handler},
	{"config", "save", config_save_handler},
	{"layer", "disable", layer_enable_handler},
	{"layer", "enable", layer_enable_handler},
	{"layer", "insert", layer_insert_handler},
	{"layer", "move", layer_move_handler},
	{"layer", "remove", layer_remove_handler},
	{"layer", "rename", layer_rename_handler},
	{"layer", "update", layer_update_handler},
	{"layer.background", "set", layer_background_set_handler},
	{"led", "set", led_set_handler},
	{"led", "update", led_update_handler},
	{"preset", "apply", preset_apply_handler},
	{"preset", "edit", preset_edit_handler},
	{"preset", "insert", preset_insert_handler},
	{"preset", "move", preset_move_handler},
	{"preset", "remove", preset_remove_handler},
	{"preset", "rename", preset_rename_handler},
	{"range", "add", range_add_handler},
	{"range", "edit", range_edit_handler},
	{"range", "remove", range_remove_handler},
	{"settings", "set", settings_set_handler},
	{NULL}
};

int ICACHE_FLASH_ATTR api_parse(struct jsonparse_state *state)
{
	char path[33], action[9];
	struct api_handler *handler;

	if (jsonparse_next(state) != '[' || jsonparse_next(state) != '"') {
		return API_ERROR_PARSE;
	}

	jsonparse_copy_value(state, path, sizeof(path));

	if (jsonparse_next(state) != ',' || jsonparse_next(state) != '"') {
		return API_ERROR_PARSE;
	}

	jsonparse_copy_value(state, action, sizeof(action));

	if (jsonparse_next(state) != ',') {
		return API_ERROR_PARSE;
	}

	handler = handlers;
	while (handler->func) {
		if (strcmp(path, handler->path) == 0 && strcmp(action, handler->action) == 0) {
			return handler->func(state, action);
		}
		handler++;
	}
	return API_FAIL;
}
