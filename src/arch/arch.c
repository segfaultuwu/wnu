#include <stddef.h>
#include <stdint.h>

#include "wnu/arch.h"
#include "wnu/keyboard.h"
#include "wnu/platform.h"

#define KERNEL_CS 0x28

struct idt_entry {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t ist;
	uint8_t type_attr;
	uint16_t offset_mid;
	uint32_t offset_high;
	uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];

extern void irq1_stub(void);

static void pic_send_eoi(unsigned irq) {
	if (irq >= 8) {
		wnu_outb(0xA0, 0x20);
	}

	wnu_outb(0x20, 0x20);
}

static void pic_remap_keyboard_only(void) {
	wnu_outb(0x21, 0xFF);
	wnu_outb(0xA1, 0xFF);

	wnu_outb(0x20, 0x11);
	wnu_io_wait();
	wnu_outb(0xA0, 0x11);
	wnu_io_wait();

	wnu_outb(0x21, 0x20);
	wnu_io_wait();
	wnu_outb(0xA1, 0x28);
	wnu_io_wait();

	wnu_outb(0x21, 0x04);
	wnu_io_wait();
	wnu_outb(0xA1, 0x02);
	wnu_io_wait();

	wnu_outb(0x21, 0x01);
	wnu_io_wait();
	wnu_outb(0xA1, 0x01);
	wnu_io_wait();

	/*
	 * Only IRQ1 keyboard.
	 * Slave fully masked.
	 */
	uint8_t master_mask = 0xFF;
	uint8_t slave_mask = 0xFF;

	master_mask &= (uint8_t)~(1u << 1);

	wnu_outb(0x21, master_mask);
	wnu_outb(0xA1, slave_mask);
}

static void set_idt_entry(uint8_t vector, void (*handler)(void)) {
	uintptr_t address = (uintptr_t)handler;

	idt[vector].offset_low = (uint16_t)(address & 0xFFFFu);
	idt[vector].selector = KERNEL_CS;
	idt[vector].ist = 0;
	idt[vector].type_attr = 0x8E;
	idt[vector].offset_mid = (uint16_t)((address >> 16) & 0xFFFFu);
	idt[vector].offset_high = (uint32_t)((address >> 32) & 0xFFFFFFFFu);
	idt[vector].zero = 0;
}

static void load_idt(void) {
	struct idt_ptr ptr = {
		.limit = sizeof(idt) - 1,
		.base = (uint64_t)(uintptr_t)idt,
	};

	__asm__ volatile("lidt %0" : : "m"(ptr) : "memory");
}

void wnu_irq1_c_handler(void) {
	uint8_t scancode = wnu_inb(0x60);

	wnu_keyboard_handle_scancode(scancode);

	pic_send_eoi(1);
}

void wnu_arch_init(void) {
	wnu_cli();

	for (size_t i = 0; i < 256; ++i) {
		idt[i] = (struct idt_entry){0};
	}

	/*
	 * IRQ1 after PIC remap = vector 0x21.
	 */
	set_idt_entry(0x21, irq1_stub);

	load_idt();
	pic_remap_keyboard_only();
}
