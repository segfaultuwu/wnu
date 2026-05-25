/* Recreated original kernel entry from src/main.c into src/kernel/main.c */

#include <limine.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "wnu/arch.h"
#include "wnu/config.h"
#include "wnu/console.h"
#include "wnu/keyboard.h"
#include "wnu/platform.h"
#include "wnu/shell.h"
#include "wnu/vfs.h"
#include "wnu/rtl8139.h"
#include "wnu/net.h"

__attribute__((used, section(".limine_reqs_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_reqs")))
static volatile LIMINE_BASE_REVISION(0);

__attribute__((used, section(".limine_reqs")))
static volatile struct limine_kernel_address_request kernel_address_request = {
	LIMINE_KERNEL_ADDRESS_REQUEST,
	.revision = 0,
};

__attribute__((used, section(".limine_reqs")))
static volatile struct limine_framebuffer_request framebuffer_request = {
	LIMINE_FRAMEBUFFER_REQUEST,
	.revision = 0,
};

__attribute__((used, section(".limine_reqs")))
static volatile struct limine_stack_size_request stack_size_request = {
	LIMINE_STACK_SIZE_REQUEST,
	.stack_size = 0x10000,
};

__attribute__((used, section(".limine_reqs_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

extern unsigned char _binary_assets_ter_u16n_psf_start[];

uint64_t wnu_kernel_virt_base;
uint64_t wnu_kernel_phys_base;

struct psf2_header {
	uint32_t magic;
	uint32_t version;
	uint32_t headersize;
	uint32_t flags;
	uint32_t glyph_count;
	uint32_t charsize;
	uint32_t height;
	uint32_t width;
} __attribute__((packed));

static struct wnu_framebuffer framebuffer;
static struct wnu_font font;

static void panic(void) {
	while (true) {
		wnu_halt();
	}
}

static void kernel_address_init(void) {
	if (kernel_address_request.response == 0) {
		panic();
	}

	wnu_kernel_virt_base = kernel_address_request.response->virtual_base;
	wnu_kernel_phys_base = kernel_address_request.response->physical_base;
}

static void font_init(void) {
	struct psf2_header *header =
		(struct psf2_header *)_binary_assets_ter_u16n_psf_start;

	if (header->magic != 0x864ab572u || header->version != 0) {
		panic();
	}

	font.data = _binary_assets_ter_u16n_psf_start + header->headersize;
	font.width = header->width;
	font.height = header->height;
	font.glyph_size = header->charsize;
	font.glyph_count = header->glyph_count;
}

static void framebuffer_init(void) {
	if (framebuffer_request.response == 0 ||
		framebuffer_request.response->framebuffer_count == 0) {
		panic();
	}

	struct limine_framebuffer *fb =
		framebuffer_request.response->framebuffers[0];

	framebuffer.base = (uint8_t *)fb->address;
	framebuffer.width = fb->width;
	framebuffer.height = fb->height;
	framebuffer.pitch = fb->pitch;
	framebuffer.bytes_per_pixel = fb->bpp / 8;
	framebuffer.red_shift = fb->red_mask_shift;
	framebuffer.green_shift = fb->green_mask_shift;
	framebuffer.blue_shift = fb->blue_mask_shift;
}

void _start(void) {
	wnu_cli();

	if (!LIMINE_BASE_REVISION_SUPPORTED) {
		panic();
	}

	if (kernel_address_request.response == 0) {
		panic();
	}

	kernel_address_init();

	framebuffer_init();
	font_init();

	wnu_console_bind(&framebuffer, &font);
	wnu_console_clear();

	wnu_vfs_init();
	rtl8139_init();
	net_init();
	wnu_console_write(BANNER);
	printf("Test %d printf()\n", 1);
	wnu_shell_init();

	wnu_keyboard_init();
	wnu_arch_init();
	wnu_sti();

	char line[256];

	while (true) {
		rtl8139_poll();

		if (wnu_console_line_ready()) {
			wnu_console_readline(line, sizeof(line));

			wnu_shell_execute(line);

			wnu_shell_print_prompt();
		}
	}
}
