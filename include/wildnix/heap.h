#ifndef WILDNIX_HEAP_H
#define WILDNIX_HEAP_H

#include <stddef.h>

void wildnix_heap_init(void);
void *wildnix_malloc(size_t size);
void wildnix_free(void *ptr);

#endif