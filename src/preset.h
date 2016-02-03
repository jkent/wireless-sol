#ifndef PRESET_H
#define PRESET_H

bool ICACHE_FLASH_ATTR preset_apply(uint8_t preset_num);
void ICACHE_FLASH_ATTR preset_apply_next(void);
int8_t ICACHE_FLASH_ATTR preset_find(const char *name);
bool ICACHE_FLASH_ATTR preset_insert(uint8_t id, struct preset *preset);
bool ICACHE_FLASH_ATTR preset_remove(uint8_t id, struct preset *preset);
bool ICACHE_FLASH_ATTR preset_move(uint8_t from, uint8_t to);

#endif /* PRESET_H */
