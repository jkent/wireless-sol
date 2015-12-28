#ifndef BUTTON_H
#define BUTTON_H

struct button_data;

typedef void (*button_function)(struct button_data *);

struct button_data {
	uint8_t level;
	uint8_t gpio;
	os_timer_t debounce;
	uint32_t down_time;
	uint32_t up_time;
	button_function down;
	button_function up;
	struct button_data *next;
};

void ICACHE_FLASH_ATTR button_add(uint8_t gpio, button_function down, button_function up);
void ICACHE_FLASH_ATTR button_init(void);

#endif /* BUTTON_H */
