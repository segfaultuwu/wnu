#ifndef WILDNIX_PLATFORM_H
#define WILDNIX_PLATFORM_H

#include <stdint.h>

static inline void wildnix_halt(void) {
    __asm__ volatile("hlt");
}

static inline void wildnix_cli(void) {
    __asm__ volatile("cli");
}

static inline void wildnix_sti(void) {
    __asm__ volatile("sti");
}

static inline void wildnix_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t wildnix_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void wildnix_io_wait(void) {
    wildnix_outb(0x80, 0);
}

#endif