#ifndef LED_H
#define LED_H

#define LED_MAX 512

#define LED_MODE_FADE (1<<0)

enum {
	LED_MODE_OFF = 0,
	LED_MODE_OFF_FADE,
	LED_MODE_LAYER,
	LED_MODE_LAYER_FADE,
	LED_MODE_SET,
	LED_MODE_SET_FADE,
};

extern uint8_t led_next[LED_MAX];
extern uint8_t led_current[LED_MAX];

void ICACHE_FLASH_ATTR led_init(void);
void led_update(void);

#endif /* LED_H */
