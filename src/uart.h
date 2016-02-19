#ifndef UART_H
#define UART_H

#include <stdarg.h>
#include <uart_hw.h>

void ICACHE_FLASH_ATTR uart0_putchar(char c);
void ICACHE_FLASH_ATTR uart1_putchar(char c);
void ICACHE_FLASH_ATTR uart_init(UartBautRate uart0_br, UartBautRate uart1_br);
void ICACHE_FLASH_ATTR uart_task_init(void);
int ICACHE_FLASH_ATTR uart0_vprintf(const char *format, va_list ap);
int ICACHE_FLASH_ATTR uart0_printf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

#endif /* UART_H */
