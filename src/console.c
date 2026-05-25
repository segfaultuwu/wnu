#include <stddef.h>
#include <stdint.h>

#include "wildnix/console.h"

#define INPUT_BUFFER_SIZE 256

static struct wildnix_framebuffer *fb;
static struct wildnix_font *font;

static size_t cursor_x;
static size_t cursor_y;
static size_t cols;
static size_t rows;

static uint32_t fg;
static uint32_t bg;
static int ready;

static char input_buffer[INPUT_BUFFER_SIZE];
static size_t input_length;
static int line_ready;

static uint32_t make_color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << fb->red_shift) |
           ((uint32_t)g << fb->green_shift) |
           ((uint32_t)b << fb->blue_shift);
}

static void put_pixel(size_t x, size_t y, uint32_t color) {
    if (!ready) {
        return;
    }

    if (x >= fb->width || y >= fb->height) {
        return;
    }

    uint8_t *pixel = fb->base + y * fb->pitch + x * fb->bytes_per_pixel;

    if (fb->bytes_per_pixel == 4) {
        *(uint32_t *)pixel = color;
    } else if (fb->bytes_per_pixel == 3) {
        pixel[0] = (uint8_t)(color & 0xff);
        pixel[1] = (uint8_t)((color >> 8) & 0xff);
        pixel[2] = (uint8_t)((color >> 16) & 0xff);
    }
}

static void draw_rect(size_t x, size_t y, size_t w, size_t h, uint32_t color) {
    for (size_t yy = 0; yy < h; ++yy) {
        for (size_t xx = 0; xx < w; ++xx) {
            put_pixel(x + xx, y + yy, color);
        }
    }
}

static void clear_cell(size_t cx, size_t cy) {
    draw_rect(
        cx * font->width,
        cy * font->height,
        font->width,
        font->height,
        bg
    );
}

static void draw_char(size_t cx, size_t cy, char ch) {
    unsigned char c = (unsigned char)ch;

    if (c >= font->glyph_count) {
        c = '?';
    }

    clear_cell(cx, cy);

    uint8_t *glyph = font->data + c * font->glyph_size;
    size_t bytes_per_row = (font->width + 7) / 8;

    size_t px = cx * font->width;
    size_t py = cy * font->height;

    for (size_t y = 0; y < font->height; ++y) {
        for (size_t x = 0; x < font->width; ++x) {
            size_t byte_index = y * bytes_per_row + x / 8;
            uint8_t byte = glyph[byte_index];
            uint8_t mask = 0x80u >> (x % 8);

            if ((byte & mask) != 0) {
                put_pixel(px + x, py + y, fg);
            }
        }
    }
}

static void scroll(void) {
    size_t line_height = font->height;
    size_t copy_height = fb->height - line_height;

    for (size_t y = 0; y < copy_height; ++y) {
        uint8_t *dst = fb->base + y * fb->pitch;
        uint8_t *src = fb->base + (y + line_height) * fb->pitch;

        for (size_t x = 0; x < fb->pitch; ++x) {
            dst[x] = src[x];
        }
    }

    draw_rect(0, copy_height, fb->width, line_height, bg);
}

static void newline(void) {
    cursor_x = 0;
    cursor_y++;

    if (cursor_y >= rows) {
        scroll();
        cursor_y = rows - 1;
    }
}

static void backspace(void) {
    if (cursor_x == 0) {
        return;
    }

    cursor_x--;
    clear_cell(cursor_x, cursor_y);
}

void wildnix_console_bind(
    struct wildnix_framebuffer *new_fb,
    struct wildnix_font *new_font
) {
    fb = new_fb;
    font = new_font;

    cursor_x = 0;
    cursor_y = 0;

    input_length = 0;
    input_buffer[0] = '\0';
    line_ready = 0;

    if (fb == 0 || font == 0 || font->width == 0 || font->height == 0) {
        ready = 0;
        return;
    }

    cols = fb->width / font->width;
    rows = fb->height / font->height;

    if (cols == 0 || rows == 0) {
        ready = 0;
        return;
    }

    fg = make_color(255, 255, 255);
    bg = make_color(0, 0, 0);

    ready = 1;
}

void wildnix_console_clear(void) {
    if (!ready) {
        return;
    }

    draw_rect(0, 0, fb->width, fb->height, bg);

    cursor_x = 0;
    cursor_y = 0;
}

void wildnix_console_putchar(char c) {
    if (!ready) {
        return;
    }

    if (c == '\n') {
        newline();
        return;
    }

    if (c == '\r') {
        cursor_x = 0;
        return;
    }

    if (c == '\b') {
        backspace();
        return;
    }

    if (c == '\t') {
        wildnix_console_putchar(' ');
        wildnix_console_putchar(' ');
        wildnix_console_putchar(' ');
        wildnix_console_putchar(' ');
        return;
    }

    draw_char(cursor_x, cursor_y, c);

    cursor_x++;

    if (cursor_x >= cols) {
        newline();
    }
}

void wildnix_console_write(const char *s) {
    if (s == 0) {
        return;
    }

    for (size_t i = 0; s[i] != '\0'; ++i) {
        wildnix_console_putchar(s[i]);
    }
}

void wildnix_console_write_len(const char *s, size_t len) {
    if (s == 0) {
        return;
    }

    for (size_t i = 0; i < len; ++i) {
        wildnix_console_putchar(s[i]);
    }
}

void wildnix_console_push_input(char c) {
    if (!ready) {
        return;
    }

    if (line_ready) {
        return;
    }

    if (c == '\n') {
        wildnix_console_putchar('\n');
        input_buffer[input_length] = '\0';
        line_ready = 1;
        return;
    }

    if (c == '\b') {
        if (input_length == 0) {
            return;
        }

        input_length--;
        input_buffer[input_length] = '\0';

        wildnix_console_putchar('\b');
        return;
    }

    if (input_length + 1 >= INPUT_BUFFER_SIZE) {
        return;
    }

    input_buffer[input_length++] = c;
    input_buffer[input_length] = '\0';

    wildnix_console_putchar(c);
}

int wildnix_console_line_ready(void) {
    return line_ready;
}

size_t wildnix_console_readline(char *buffer, size_t size) {
    if (buffer == 0 || size == 0) {
        return 0;
    }

    if (!line_ready) {
        buffer[0] = '\0';
        return 0;
    }

    size_t copy_len = input_length;

    if (copy_len >= size) {
        copy_len = size - 1;
    }

    for (size_t i = 0; i < copy_len; ++i) {
        buffer[i] = input_buffer[i];
    }

    buffer[copy_len] = '\0';

    input_length = 0;
    input_buffer[0] = '\0';
    line_ready = 0;

    return copy_len;
}