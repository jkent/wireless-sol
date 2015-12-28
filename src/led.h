#ifndef LED_H
#define LED_H

#define NUM_LEDS 120

unsigned char led_current[NUM_LEDS];

void ICACHE_FLASH_ATTR led_init(void);
void led_update(void);

#endif /* LED_H */
