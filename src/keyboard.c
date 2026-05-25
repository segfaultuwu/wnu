#include <stdbool.h>
#include <stdint.h>

#include "wnu/console.h"
#include "wnu/keyboard.h"
#include "wnu/platform.h"

static bool left_shift;
static bool right_shift;
static bool caps_lock;
static bool extended;

static bool ps2_wait_input(void) {
    for (uint64_t i = 0; i < 1000000; ++i) {
        if ((wnu_inb(0x64) & 0x02u) == 0) {
            return true;
        }
    }

    return false;
}

static bool ps2_wait_output(void) {
    for (uint64_t i = 0; i < 1000000; ++i) {
        if ((wnu_inb(0x64) & 0x01u) != 0) {
            return true;
        }
    }

    return false;
}

static void ps2_flush(void) {
    for (uint64_t i = 0; i < 1024; ++i) {
        if ((wnu_inb(0x64) & 0x01u) == 0) {
            return;
        }

        (void)wnu_inb(0x60);
    }
}

static bool ps2_write_command(uint8_t command) {
    if (!ps2_wait_input()) {
        return false;
    }

    wnu_outb(0x64, command);
    return true;
}

static bool ps2_write_data(uint8_t data) {
    if (!ps2_wait_input()) {
        return false;
    }

    wnu_outb(0x60, data);
    return true;
}

static bool ps2_read_data(uint8_t *out) {
    if (!ps2_wait_output()) {
        return false;
    }

    *out = wnu_inb(0x60);
    return true;
}

static bool keyboard_write(uint8_t data) {
    uint8_t response;

    if (!ps2_write_data(data)) {
        return false;
    }

    if (!ps2_read_data(&response)) {
        return false;
    }

    return response == 0xFA;
}

static char scancode_to_ascii(uint8_t scancode) {
    bool shifted = left_shift || right_shift;
    bool upper = shifted ^ caps_lock;

    switch (scancode) {
        case 0x02: return shifted ? '!' : '1';
        case 0x03: return shifted ? '@' : '2';
        case 0x04: return shifted ? '#' : '3';
        case 0x05: return shifted ? '$' : '4';
        case 0x06: return shifted ? '%' : '5';
        case 0x07: return shifted ? '^' : '6';
        case 0x08: return shifted ? '&' : '7';
        case 0x09: return shifted ? '*' : '8';
        case 0x0A: return shifted ? '(' : '9';
        case 0x0B: return shifted ? ')' : '0';

        case 0x0C: return shifted ? '_' : '-';
        case 0x0D: return shifted ? '+' : '=';

        case 0x10: return upper ? 'Q' : 'q';
        case 0x11: return upper ? 'W' : 'w';
        case 0x12: return upper ? 'E' : 'e';
        case 0x13: return upper ? 'R' : 'r';
        case 0x14: return upper ? 'T' : 't';
        case 0x15: return upper ? 'Y' : 'y';
        case 0x16: return upper ? 'U' : 'u';
        case 0x17: return upper ? 'I' : 'i';
        case 0x18: return upper ? 'O' : 'o';
        case 0x19: return upper ? 'P' : 'p';

        case 0x1E: return upper ? 'A' : 'a';
        case 0x1F: return upper ? 'S' : 's';
        case 0x20: return upper ? 'D' : 'd';
        case 0x21: return upper ? 'F' : 'f';
        case 0x22: return upper ? 'G' : 'g';
        case 0x23: return upper ? 'H' : 'h';
        case 0x24: return upper ? 'J' : 'j';
        case 0x25: return upper ? 'K' : 'k';
        case 0x26: return upper ? 'L' : 'l';

        case 0x27: return shifted ? ':' : ';';
        case 0x28: return shifted ? '"' : '\'';
        case 0x29: return shifted ? '~' : '`';
        case 0x2B: return shifted ? '|' : '\\';

        case 0x2C: return upper ? 'Z' : 'z';
        case 0x2D: return upper ? 'X' : 'x';
        case 0x2E: return upper ? 'C' : 'c';
        case 0x2F: return upper ? 'V' : 'v';
        case 0x30: return upper ? 'B' : 'b';
        case 0x31: return upper ? 'N' : 'n';
        case 0x32: return upper ? 'M' : 'm';

        case 0x33: return shifted ? '<' : ',';
        case 0x34: return shifted ? '>' : '.';
        case 0x35: return shifted ? '?' : '/';

        case 0x39: return ' ';

        default: return 0;
    }
}

void wnu_keyboard_init(void) {
    left_shift = false;
    right_shift = false;
    caps_lock = false;
    extended = false;

    ps2_write_command(0xAD);
    ps2_flush();

    if (!ps2_write_command(0x20)) {
        return;
    }

    uint8_t config;

    if (!ps2_read_data(&config)) {
        return;
    }

    config |= (1u << 0);
    config &= (uint8_t)~(1u << 1);
    config |= (1u << 6);

    if (!ps2_write_command(0x60)) {
        return;
    }

    if (!ps2_write_data(config)) {
        return;
    }

    ps2_write_command(0xAE);
    ps2_flush();

    keyboard_write(0xF4);
}

void wnu_keyboard_handle_scancode(uint8_t data) {
    if (data == 0xE0) {
        extended = true;
        return;
    }

    if (extended) {
        extended = false;
        return;
    }

    bool released = (data & 0x80u) != 0;
    uint8_t scancode = data & 0x7Fu;

    if (released) {
        if (scancode == 0x2A) {
            left_shift = false;
        } else if (scancode == 0x36) {
            right_shift = false;
        }

        return;
    }

    if (scancode == 0x2A) {
        left_shift = true;
        return;
    }

    if (scancode == 0x36) {
        right_shift = true;
        return;
    }

    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }

    if (scancode == 0x1C) {
        wnu_console_push_input('\n');
        return;
    }

    if (scancode == 0x0E) {
        wnu_console_push_input('\b');
        return;
    }

    char c = scancode_to_ascii(scancode);

    if (c != 0) {
        wnu_console_push_input(c);
    }
}