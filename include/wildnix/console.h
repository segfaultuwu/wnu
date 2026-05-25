#ifndef WILDNIX_CONSOLE_H
#define WILDNIX_CONSOLE_H

#include <stddef.h>
#include <stdint.h>

struct wildnix_framebuffer {
    uint8_t *base;
    size_t width;
    size_t height;
    size_t pitch;
    size_t bytes_per_pixel;
    uint8_t red_shift;
    uint8_t green_shift;
    uint8_t blue_shift;
};

struct wildnix_font {
    uint8_t *data;
    uint32_t width;
    uint32_t height;
    uint32_t glyph_size;
    uint32_t glyph_count;
};

void wildnix_console_bind(
    struct wildnix_framebuffer *fb,
    struct wildnix_font *font
);

void wildnix_console_clear(void);

void wildnix_console_putchar(char c);
void wildnix_console_write(const char *s);
void wildnix_console_write_len(const char *s, size_t len);

/*
 * Input line API.
 *
 * Keyboard driver calls wildnix_console_push_input().
 * Kernel main loop checks wildnix_console_line_ready().
 * Then it reads the full line with wildnix_console_readline().
 */
void wildnix_console_push_input(char c);
int wildnix_console_line_ready(void);
size_t wildnix_console_readline(char *buffer, size_t size);

#endif