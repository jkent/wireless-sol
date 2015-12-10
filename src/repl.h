#ifndef REPL_H
#define REPL_H

typedef struct repl_command repl_command;

struct repl_command {
    const char *name;
    void (*handler)(char *args);
};

void ICACHE_FLASH_ATTR repl_init(void);
void ICACHE_FLASH_ATTR repl_process_char(char c);

#endif /* REPL_H */
