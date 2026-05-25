#include <stddef.h>
#include <stdint.h>

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