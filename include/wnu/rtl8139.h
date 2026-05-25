#ifndef WNU_RTL8139_H
#define WNU_RTL8139_H

#include <stddef.h>
#include <stdint.h>

void rtl8139_init(void);
void rtl8139_poll(void);

void rtl8139_send(const uint8_t *data, size_t len);
const uint8_t *rtl8139_get_mac(void);

#endif