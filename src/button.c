#include <esp8266.h>
#include "button.h"

#define BUTTON_TASK_ID 1

static os_event_t button_evt_queue[2];
static struct button_data *first_button = NULL;
static struct button_data *last_button = NULL;

void ICACHE_FLASH_ATTR button_task_handler(os_event_t *evt);
static void button_intr_handler(void);

void ICACHE_FLASH_ATTR button_add(uint8_t gpio, button_function down,
		button_function up)
{
	struct button_data *button = (struct button_data *)zalloc(
			sizeof(struct button_data));

	button->gpio = gpio;
	button->down = down;
	button->up = up;

	if (!first_button) {
		first_button = button;
		last_button = button;
	}
	else {
		last_button->next = button;
		last_button = button;
	}
}

void ICACHE_FLASH_ATTR button_init(void)
{
	struct button_data *button = first_button;
	uint32_t gpio_base;
	uint8_t gpio_func;

	ETS_GPIO_INTR_DISABLE();
	ETS_GPIO_INTR_ATTACH(button_intr_handler, NULL);

	system_os_task(button_task_handler, BUTTON_TASK_ID, button_evt_queue,
			sizeof(button_evt_queue) / sizeof(*button_evt_queue));

	while (button) {
		switch (button->gpio) {
		case 0:
			gpio_base = PERIPHS_IO_MUX_GPIO0_U;
			gpio_func = FUNC_GPIO0;
			break;
		case 1:
			gpio_base = PERIPHS_IO_MUX_U0TXD_U;
			gpio_func = FUNC_GPIO1;
			break;
		case 2:
			gpio_base = PERIPHS_IO_MUX_GPIO2_U;
			gpio_func = FUNC_GPIO2;
			break;
		case 3:
			gpio_base = PERIPHS_IO_MUX_U0RXD_U;
			gpio_func = FUNC_GPIO3;
			break;
		case 4:
			gpio_base = PERIPHS_IO_MUX_GPIO4_U;
			gpio_func = FUNC_GPIO4;
			break;
		case 5:
			gpio_base = PERIPHS_IO_MUX_GPIO5_U;
			gpio_func = FUNC_GPIO5;
			break;
		case 9:
			gpio_base = PERIPHS_IO_MUX_SD_DATA2_U;
			gpio_func = FUNC_GPIO9;
			break;
		case 10:
			gpio_base = PERIPHS_IO_MUX_SD_DATA3_U;
			gpio_func = FUNC_GPIO10;
			break;
		case 12:
			gpio_base = PERIPHS_IO_MUX_MTDI_U;
			gpio_func = FUNC_GPIO12;
			break;
		case 13:
			gpio_base = PERIPHS_IO_MUX_MTCK_U;
			gpio_func = FUNC_GPIO13;
			break;
		case 14:
			gpio_base = PERIPHS_IO_MUX_MTMS_U;
			gpio_func = FUNC_GPIO14;
			break;
		case 15:
			gpio_base = PERIPHS_IO_MUX_MTDO_U;
			gpio_func = FUNC_GPIO15;
			break;
		default:
			continue;
		}

		button->level = 1;

		// set pin as GPIO
		PIN_FUNC_SELECT(gpio_base, gpio_func);

		// enable pullup
		PIN_PULLUP_EN(gpio_base);

		// set pin as input with interrupts disabled
		gpio_output_set(0, 0, 0, button->gpio);
		gpio_register_set(button->gpio,
				GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE) |
				GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE) |
				GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));

		// clear pin status
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(button->gpio));

		// enable negedge pin interrupt
		gpio_pin_intr_state_set(button->gpio, GPIO_PIN_INTR_NEGEDGE);

		button = button->next;
	}

	ETS_GPIO_INTR_ENABLE();
}

static void ICACHE_FLASH_ATTR button_50ms_cb(struct button_data *button)
{
	os_timer_disarm(&button->debounce);

	// high, then key is up
	if (GPIO_INPUT_GET(button->gpio) == 1) {
		button->level = 1;
		button->up_time = system_get_time();
		if (button->up) {
			button->up(button);
		}
		gpio_pin_intr_state_set(button->gpio, GPIO_PIN_INTR_NEGEDGE);
	}
	else {
		gpio_pin_intr_state_set(button->gpio, GPIO_PIN_INTR_POSEDGE);
	}
}

void ICACHE_FLASH_ATTR button_task_handler(os_event_t *evt)
{
	struct button_data *button = (struct button_data *)evt->par;

	if (button->down) {
		button->down(button);
	}
}

static void button_intr_handler(void)
{
	struct button_data *button = first_button;
	uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

	while (button) {
		if (!(gpio_status & BIT(button->gpio))) {
			continue;
		}

		// disable pin interrupt
		gpio_pin_intr_state_set(button->gpio, GPIO_PIN_INTR_DISABLE);

		// clear gpio status
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(button->gpio));

		if (button->level) {
			button->level = 0;
			button->down_time = system_get_time();
			system_os_post(BUTTON_TASK_ID, 0, (ETSParam)button);
			gpio_pin_intr_state_set(button->gpio, GPIO_PIN_INTR_POSEDGE);
		}
		else {
			// debounce timer
			os_timer_disarm(&button->debounce);
			os_timer_setfn(&button->debounce, (os_timer_func_t *)button_50ms_cb,
					button);
			os_timer_arm(&button->debounce, 50, 0);
		}

		button = button->next;
	}
}
