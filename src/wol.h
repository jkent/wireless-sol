#ifndef WOL_H
#define WOL_H

void wol_init(void);
void wol_cleanup(void);
void wol_send(const uint8_t *mac);

#endif /* WOL_H */
