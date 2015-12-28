#include <esp8266.h>
#include "led.h"

static inline uint32_t _getCycleCount(void)
{
	uint32_t cycles;
	__asm__ __volatile__("rsr %0,ccount":"=a" (cycles));
	return cycles;
}

void ICACHE_FLASH_ATTR led_init(void)
{
	uint32_t res, start_time;

	res = (1000 * system_get_cpu_freq()) / 20; // 50us

	PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO5_U);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	GPIO_OUTPUT_SET(5, 0);

	bzero(led_current, sizeof(led_current));

	led_update();

	start_time = _getCycleCount();
	while ((_getCycleCount() - start_time) < res)
		;

	led_update();
}

void led_update(void)
{
	unsigned char bit, byte;
	int i, j;
	uint32_t t0h, t1h, ttot, t, c, start_time;

	ets_intr_lock();

	start_time = _getCycleCount();

	t0h = (1000 * system_get_cpu_freq()) / 2500; // 0.4us
	t1h = (1000 * system_get_cpu_freq()) / 1250; // 0.8us
	ttot = (1000 * system_get_cpu_freq()) / 800; // 1.25us

	for (i = 0; i < NUM_LEDS; i++) {
		byte = led_current[i];
		for (j = 0; j < 3; j++) {
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
	}

	ets_intr_unlock();
}
