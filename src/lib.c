#include "wnu/console.h"
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "wnu/console.h"

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;

    for (size_t i = 0; i < n; ++i) {
        d[i] = s[i];
    }

    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;

    if (d < s) {
        for (size_t i = 0; i < n; ++i) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }

    return dst;
}

void *memset(void *dst, int value, size_t n) {
    uint8_t *d = dst;

    for (size_t i = 0; i < n; ++i) {
        d[i] = (uint8_t)value;
    }

    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = a;
    const uint8_t *y = b;

    for (size_t i = 0; i < n; ++i) {
        if (x[i] != y[i]) {
            return x[i] < y[i] ? -1 : 1;
        }
    }

    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;

    while (s[len] != '\0') {
        len++;
    }

    return len;
}

int strcmp(const char *a, const char *b) {
    size_t i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }

        i++;
    }

    return (unsigned char)a[i] - (unsigned char)b[i];
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }

        if (a[i] == '\0') {
            return 0;
        }
    }

    return 0;
}

char *strchr(const char *s, int c) {
    while (*s != '\0') {
        if (*s == (char)c) {
            return (char *)s;
        }

        s++;
    }

    if (c == '\0') {
        return (char *)s;
    }

    return 0;
}

char *strerror(int errnum) {
    (void)errnum;
    return "kernel error";
}

void abort(void) {
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

static void print_unsigned_base(uint64_t value, uint32_t base) {
    char buffer[32];
    size_t i = 0;

    if (value == 0) {
        wnu_console_putchar('0');
        return;
    }

    while (value > 0) {
        uint64_t digit = value % base;

        if (digit < 10) {
            buffer[i++] = (char)('0' + digit);
        } else {
            buffer[i++] = (char)('a' + digit - 10);
        }

        value /= base;
    }

    while (i > 0) {
        wnu_console_putchar(buffer[--i]);
    }
}

static void print_signed(int64_t value) {
    if (value < 0) {
        wnu_console_putchar('-');
        print_unsigned_base((uint64_t)(-value), 10);
        return;
    }

    print_unsigned_base((uint64_t)value, 10);
}

void printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    for (size_t i = 0; format[i] != '\0'; ++i) {
        if (format[i] != '%') {
            wnu_console_putchar(format[i]);
            continue;
        }

        i++;

        if (format[i] == '\0') {
            break;
        }

        switch (format[i]) {
            case '%':
                wnu_console_putchar('%');
                break;

            case 's': {
                const char *s = va_arg(args, const char *);

                if (s == 0) {
                    s = "(null)";
                }

                wnu_console_write(s);
                break;
            }

            case 'c': {
                char c = (char)va_arg(args, int);
                wnu_console_putchar(c);
                break;
            }

            case 'd':
            case 'i': {
                int value = va_arg(args, int);
                print_signed(value);
                break;
            }

            case 'u': {
                unsigned int value = va_arg(args, unsigned int);
                print_unsigned_base(value, 10);
                break;
            }

            case 'x': {
                unsigned int value = va_arg(args, unsigned int);
                print_unsigned_base(value, 16);
                break;
            }

            case 'p': {
                uintptr_t value = (uintptr_t)va_arg(args, void *);
                wnu_console_write("0x");
                print_unsigned_base(value, 16);
                break;
            }

            default:
                wnu_console_putchar('%');
                wnu_console_putchar(format[i]);
                break;
        }
    }

    va_end(args);
}
