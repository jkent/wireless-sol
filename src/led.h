#ifndef LED_H
#define LED_H

#define LED_MAX 512

enum {
	LED_MODE_DATA,
	LED_MODE_LAYER,
};

extern uint8_t led_current[LED_MAX];
extern uint8_t led_next[LED_MAX];

void ICACHE_FLASH_ATTR led_init(void);
void led_update(void);

#endif /* LED_H */
