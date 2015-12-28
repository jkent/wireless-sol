#include <esp8266.h>
#include "data.h"

struct flash_data flash_data;

void ICACHE_FLASH_ATTR data_init(void)
{
	/* load data from flash */
	/* if CRC fail { */
	memset(&flash_data, 0, sizeof(flash_data));
	/* } */

	/* test data: */
	flash_data.led_count = 120;
	flash_data.led_mode = LED_MODE_LAYER;

	flash_data.layer_background = 255;

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
