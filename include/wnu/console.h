#ifndef WNU_CONSOLE_H
#define WNU_CONSOLE_H

#include <stddef.h>
#include <stdint.h>

struct wnu_framebuffer {
    uint8_t *base;
    size_t width;
    size_t height;
    size_t pitch;
    size_t bytes_per_pixel;
    uint8_t red_shift;
    uint8_t green_shift;
    uint8_t blue_shift;
};

struct wnu_font {
    uint8_t *data;
    uint32_t width;
    uint32_t height;
    uint32_t glyph_size;
    uint32_t glyph_count;
};

void wnu_console_bind(
    struct wnu_framebuffer *fb,
    struct wnu_font *font
);

void wnu_console_clear(void);

void wnu_console_putchar(char c);
void wnu_console_write(const char *s);
void wnu_console_write_len(const char *s, size_t len);

/*
 * Input line API.
 *
 * Keyboard driver calls wnu_console_push_input().
 * Kernel main loop checks wnu_console_line_ready().
 * Then it reads the full line with wnu_console_readline().
 */
void wnu_console_push_input(char c);
int wnu_console_line_ready(void);
size_t wnu_console_readline(char *buffer, size_t size);

#endif