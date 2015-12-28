#include <esp8266.h>
#include "json/jsontree.h"
#include "led.h"

void ICACHE_FLASH_ATTR json_led_status(int (*putchar)(int))
{
	struct jsontree_context json;
	json.putchar = putchar;

	putchar('[');
	for (int i = 0; i < NUM_LEDS; i++) {
		jsontree_write_int(&json, led_current[i]);
		if (i + 1 < NUM_LEDS) {
			putchar(',');
		}
	}
	putchar(']');
}
