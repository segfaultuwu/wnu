#include <stddef.h>
#include <stdint.h>

#include "wnu/heap.h"

#define HEAP_SIZE (1024 * 1024)

static uint8_t heap[HEAP_SIZE];
static size_t heap_offset;

void wnu_heap_init(void) {
	heap_offset = 0;
}

static size_t align_up(size_t value, size_t align) {
	return (value + align - 1) & ~(align - 1);
}

void *wnu_malloc(size_t size) {
	if (size == 0) {
		return 0;
	}

	heap_offset = align_up(heap_offset, 16);

	if (heap_offset + size > HEAP_SIZE) {
		return 0;
	}

	void *ptr = &heap[heap_offset];
	heap_offset += size;

	return ptr;
}

void wnu_free(void *ptr) {
	(void)ptr;
}
