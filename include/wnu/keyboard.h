#ifndef WNU_KEYBOARD_H
#define WNU_KEYBOARD_H

#include <stdint.h>

void wnu_keyboard_init(void);
void wnu_keyboard_handle_scancode(uint8_t scancode);

#endif