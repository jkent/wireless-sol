#ifndef USER_INIT_H
#define USER_INIT_H

#ifndef ets_vsnprintf
#include <stdarg.h>
int ets_vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif

#ifndef ets_intr_lock
void ets_intr_lock(void);
void ets_intr_unlock(void);
#endif

#ifndef FUNC_U0CTS
#define FUNC_U0CTS 4
#endif

#define zalloc(s) os_zalloc(s)
#define calloc(n, s) os_calloc(n * s)
#define realloc(p, s) os_realloc(p, s)

#undef putchar

#endif /* USER_INIT_H */
