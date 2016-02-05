#include <esp8266.h>
#include "layer.h"
#include "led.h"
#include "data.h"
#include "api.h"

uint8_t led_next[LED_MAX] = {0};
uint8_t led_current[LED_MAX] = {0};

static const uint8_t linear_map[256] = {
	  0,   1,   1,   1,   1,   1,   1,   1,
	  1,   1,   1,   1,   1,   1,   1,   2,
	  2,   2,   2,   2,   2,   2,   2,   2,
	  2,   3,   3,   3,   3,   3,   3,   3,
	  4,   4,   4,   4,   4,   4,   5,   5,
	  5,   5,   5,   6,   6,   6,   6,   6,
	  7,   7,   7,   7,   8,   8,   8,   8,
	  9,   9,   9,  10,  10,  10,  11,  11,
	 11,  11,  12,  12,  12,  13,  13,  14,
	 14,  14,  15,  15,  16,  16,  16,  17,
	 17,  18,  18,  19,  19,  19,  20,  20,
	 21,  21,  22,  22,  23,  23,  24,  24,
	 25,  26,  26,  27,  27,  28,  29,  29,
	 30,  30,  31,  32,  32,  33,  34,  34,
	 35,  36,  36,  37,  38,  39,  39,  40,
	 41,  42,  42,  43,  44,  45,  46,  46,
	 47,  48,  49,  50,  51,  52,  52,  53,
	 54,  55,  56,  57,  58,  59,  60,  61,
	 62,  63,  64,  65,  66,  67,  68,  69,
	 70,  72,  73,  74,  75,  76,  77,  78,
	 80,  81,  82,  83,  84,  86,  87,  88,
	 90,  91,  92,  93,  95,  96,  98,  99,
	100, 102, 103, 105, 106, 107, 109, 110,
	112, 113, 115, 116, 118, 119, 121, 123,
	124, 126, 127, 129, 131, 132, 134, 136,
	137, 139, 141, 143, 144, 146, 148, 150,
	152, 153, 155, 157, 159, 161, 163, 165,
	167, 169, 171, 173, 175, 177, 179, 181,
	183, 185, 187, 189, 191, 193, 196, 198,
	200, 202, 204, 207, 209, 211, 213, 216,
	218, 220, 223, 225, 228, 230, 232, 235,
	237, 240, 242, 245, 247, 250, 252, 255 };

static inline uint32_t _getCycleCount(void)
{
	uint32_t cycles;
	__asm__ __volatile__("rsr %0,ccount":"=a" (cycles));
	return cycles;
}

void ICACHE_FLASH_ATTR led_init(void)
{
	uint32_t res, start_time;

	/* Setup GPIO */
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO5_U);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	GPIO_OUTPUT_SET(5, 0);

	/* Init LED data */
	layer_update(false);

	/* Wait for refresh */
	start_time = _getCycleCount();
	res = (1000 * system_get_cpu_freq()) / 20; // 50us
	while ((_getCycleCount() - start_time) < res)
		;

	/* Set LEDs */
	led_update();
}

void led_update(void)
{
	uint8_t byte, component, bit;
	uint16_t i;
	uint32_t start_time, t0h, t1h, ttot, t, c;

	start_time = _getCycleCount();

	t0h = (1000 * system_get_cpu_freq()) / 2500; // 0.4us
	t1h = (1000 * system_get_cpu_freq()) / 1250; // 0.8us
	ttot = (1000 * system_get_cpu_freq()) / 800; // 1.25us

	for (i = 0; i < config_data.led_count; i++) {
		byte = linear_map[led_current[i]];
		ets_intr_lock();
		for (component = 0; component < 3; component++) {
			for (bit = 0; bit < 8; bit++) {
				t = (byte & (1 << (7 - bit))) ? t1h : t0h;
				while (((c = _getCycleCount()) - start_time) < ttot)
					;
				GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << 5);
				start_time = c;
				while (((c = _getCycleCount()) - start_time) < t)
					;
				GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << 5);
			}
		}
		ets_intr_unlock();
	}
	needs_led_update = false;
}
