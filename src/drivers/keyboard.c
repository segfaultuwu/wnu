#include <stddef.h>
#include <stdint.h>

#include "wnu/keyboard.h"
#include "wnu/console.h"

static const char keymap[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r',
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n',
    'm', ',', '.', '/', 0, '*', 0, ' ', 0
};

void wnu_keyboard_handle_scancode(uint8_t sc) {
    if (sc & 0x80) {
        return;
    }

    if (sc >= sizeof(keymap)) {
        return;
    }

    char c = keymap[sc];

    if (c == 0) {
        return;
    }

    wnu_console_push_input(c);
}

void wnu_keyboard_init(void) {
    /* No special initialization needed for now. */
}
