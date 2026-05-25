#ifndef WILDNIX_KEYBOARD_H
#define WILDNIX_KEYBOARD_H

#include <stdint.h>

void wildnix_keyboard_init(void);
void wildnix_keyboard_handle_scancode(uint8_t scancode);

#endif