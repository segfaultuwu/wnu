#ifndef WNU_VFS_H
#define WNU_VFS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define WNU_VFS_MAX_NODES 128
#define WNU_VFS_MAX_NAME  64
#define WNU_VFS_MAX_DATA  4096

typedef enum {
    WNU_VFS_FILE,
    WNU_VFS_DIR
} wnu_vfs_node_type;

struct wnu_vfs_node {
    int used;

    uint64_t inode;
    wnu_vfs_node_type type;

    char path[WNU_VFS_MAX_NAME];

    uint8_t data[WNU_VFS_MAX_DATA];
    size_t size;
};

void wnu_vfs_init(void);

int wnu_vfs_exists(const char *path);
int wnu_vfs_is_dir(const char *path);
int wnu_vfs_is_file(const char *path);
int wnu_vfs_create_file(const char *path);
int wnu_vfs_create_dir(const char *path);
int wnu_vfs_delete(const char *path);

int wnu_vfs_write_file(const char *path, const uint8_t *data, size_t size);
int wnu_vfs_append_file(const char *path, const uint8_t *data, size_t size);

const uint8_t *wnu_vfs_read_file(const char *path, size_t *out_size);

static void print_indent(int depth) {
    for (int i = 0; i < depth; ++i) {
        printf("  ");
    }
}

void wnu_vfs_tree(const char *path);

void wnu_vfs_list_dir(const char *path);

#endif
