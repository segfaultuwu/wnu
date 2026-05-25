#ifndef WNU_HEAP_H
#define WNU_HEAP_H

#include <stddef.h>

void wnu_heap_init(void);
void *wnu_malloc(size_t size);
void wnu_free(void *ptr);

#endif