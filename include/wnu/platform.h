#ifndef WNU_PLATFORM_H
#define WNU_PLATFORM_H

#include <stdint.h>

extern uint64_t wnu_kernel_virt_base;
extern uint64_t wnu_kernel_phys_base;

static inline uintptr_t wnu_virt_to_phys(const void *ptr) {
    uintptr_t virt = (uintptr_t)ptr;
    return virt - wnu_kernel_virt_base + wnu_kernel_phys_base;
}

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

static inline void wnu_outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t wnu_inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void wnu_io_wait(void) {
    wnu_outb(0x80, 0);
}

#endif
