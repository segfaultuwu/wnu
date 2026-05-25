#include <stddef.h>
#include <string.h>

#include "wnu/block.h"
#include "wnu/console.h"
#include "wnu/vfs.h"

#define MAX_BLOCK_DEV 8

static struct wnu_block_device *devices[MAX_BLOCK_DEV];

int wnu_block_register(struct wnu_block_device *dev) {
    for (int i = 0; i < MAX_BLOCK_DEV; ++i) {
        if (devices[i] == NULL) {
            devices[i] = dev;
            /* Ensure /dev exists in VFS */
            if (!wnu_vfs_exists("/dev")) {
                wnu_vfs_create_dir("/dev");
            }

            /* Create /dev/<name> file to represent the device */
            char path[WNU_VFS_MAX_NAME];
            size_t p = 0;
            path[p++] = '/';
            path[p++] = 'd';
            path[p++] = 'e';
            path[p++] = 'v';
            path[p++] = '/';

            /* copy device name */
            for (size_t j = 0; j < sizeof(dev->name) && p + 1 < sizeof(path); ++j) {
                char c = dev->name[j];
                if (c == '\0') break;
                path[p++] = c;
            }

            path[p] = '\0';

            if (wnu_vfs_create_file(path)) {
                const char *msg = "block device";
                wnu_vfs_write_file(path, (const uint8_t *)msg, 12);
            }

            return 1;
        }
    }
    return 0;
}

struct wnu_block_device *wnu_block_find(const char *name) {
    const char *lookup = name;
    /* Accept names like "/dev/sata0" by stripping the /dev/ prefix */
    if (lookup[0] == '/' && lookup[1] == 'd' && lookup[2] == 'e' && lookup[3] == 'v' && lookup[4] == '/') {
        lookup += 5;
    }

    for (int i = 0; i < MAX_BLOCK_DEV; ++i) {
        if (devices[i] == NULL) continue;
        if (strncmp(devices[i]->name, lookup, sizeof(devices[i]->name)) == 0) {
            return devices[i];
        }
    }
    return NULL;
}

int wnu_block_read(const char *name, uint64_t lba, void *buf, size_t count) {
    struct wnu_block_device *d = wnu_block_find(name);
    if (d == NULL) return 0;
    if (d->read == NULL) return 0;
    return d->read(d->driver_data, lba, buf, count);
}
