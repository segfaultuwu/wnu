#ifndef WNU_BLOCK_H
#define WNU_BLOCK_H

#include <stddef.h>
#include <stdint.h>

#define WNU_BLOCK_NAME_MAX 32

typedef int (*wnu_block_read_fn)(void *driver_data, uint64_t lba, void *buf, size_t count);

struct wnu_block_device {
    char name[WNU_BLOCK_NAME_MAX];
    void *driver_data;
    wnu_block_read_fn read;
    uint64_t capacity; /* in sectors */
};

int wnu_block_register(struct wnu_block_device *dev);
struct wnu_block_device *wnu_block_find(const char *name);
int wnu_block_read(const char *name, uint64_t lba, void *buf, size_t count);

#endif
