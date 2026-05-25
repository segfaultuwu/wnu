#ifndef WILDNIX_VFS_H
#define WILDNIX_VFS_H

#include <stddef.h>
#include <stdint.h>

#define WILDNIX_VFS_MAX_NODES 128
#define WILDNIX_VFS_MAX_NAME  64
#define WILDNIX_VFS_MAX_DATA  4096

typedef enum {
    WILDNIX_VFS_FILE,
    WILDNIX_VFS_DIR
} wildnix_vfs_node_type;

struct wildnix_vfs_node {
    int used;

    uint64_t inode;
    wildnix_vfs_node_type type;

    char path[WILDNIX_VFS_MAX_NAME];

    uint8_t data[WILDNIX_VFS_MAX_DATA];
    size_t size;
};

void wildnix_vfs_init(void);

int wildnix_vfs_exists(const char *path);
int wildnix_vfs_is_dir(const char *path);
int wildnix_vfs_is_file(const char *path);
int wildnix_vfs_create_file(const char *path);
int wildnix_vfs_create_dir(const char *path);
int wildnix_vfs_delete(const char *path);

int wildnix_vfs_write_file(const char *path, const uint8_t *data, size_t size);
int wildnix_vfs_append_file(const char *path, const uint8_t *data, size_t size);

const uint8_t *wildnix_vfs_read_file(const char *path, size_t *out_size);

void wildnix_vfs_list_dir(const char *path);

#endif
