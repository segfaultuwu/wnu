#ifndef WNU_PLATFORM_H
#define WNU_PLATFORM_H

#include <stdint.h>

static inline void wnu_halt(void) {
    __asm__ volatile("hlt");
}

static inline void wnu_cli(void) {
    __asm__ volatile("cli");
}

static inline void wnu_sti(void) {
    __asm__ volatile("sti");
}

static inline void wnu_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t wnu_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void wnu_io_wait(void) {
    wnu_outb(0x80, 0);
}

#endif