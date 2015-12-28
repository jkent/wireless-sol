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

static int ICACHE_FLASH_ATTR led_status_callback(struct jsontree_context *path)
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

const struct jsontree_callback json_led_status_callback =
	JSONTREE_CALLBACK(led_status_callback, NULL);
