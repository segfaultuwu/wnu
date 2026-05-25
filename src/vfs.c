#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wildnix/console.h"
#include "wildnix/vfs.h"

static struct wildnix_vfs_node nodes[WILDNIX_VFS_MAX_NODES];
static uint64_t next_inode;

static size_t string_len(const char *s) {
    size_t len = 0;

    while (s[len] != '\0') {
        len++;
    }

    return len;
}

static int string_equal(const char *a, const char *b) {
    size_t i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }

        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

static int string_starts_with(const char *s, const char *prefix) {
    size_t i = 0;

    while (prefix[i] != '\0') {
        if (s[i] != prefix[i]) {
            return 0;
        }

        i++;
    }

    return 1;
}

static void string_copy(char *dst, const char *src, size_t max) {
    size_t i = 0;

    if (max == 0) {
        return;
    }

    while (i + 1 < max && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static struct wildnix_vfs_node *find_node(const char *path) {
    for (size_t i = 0; i < WILDNIX_VFS_MAX_NODES; ++i) {
        if (!nodes[i].used) {
            continue;
        }

        if (string_equal(nodes[i].path, path)) {
            return &nodes[i];
        }
    }

    return 0;
}

static struct wildnix_vfs_node *alloc_node(void) {
    for (size_t i = 0; i < WILDNIX_VFS_MAX_NODES; ++i) {
        if (!nodes[i].used) {
            nodes[i].used = 1;
            nodes[i].inode = next_inode++;
            nodes[i].size = 0;
            nodes[i].path[0] = '\0';
            return &nodes[i];
        }
    }

    return 0;
}

static int valid_path(const char *path) {
    if (path == 0) {
        return 0;
    }

    if (path[0] != '/') {
        return 0;
    }

    if (string_len(path) >= WILDNIX_VFS_MAX_NAME) {
        return 0;
    }

    return 1;
}

static void parent_path(const char *path, char *out, size_t out_size) {
    size_t len = string_len(path);

    if (len == 0 || string_equal(path, "/")) {
        string_copy(out, "/", out_size);
        return;
    }

    size_t slash = 0;

    for (size_t i = 0; i < len; ++i) {
        if (path[i] == '/') {
            slash = i;
        }
    }

    if (slash == 0) {
        string_copy(out, "/", out_size);
        return;
    }

    size_t i = 0;

    while (i < slash && i + 1 < out_size) {
        out[i] = path[i];
        i++;
    }

    out[i] = '\0';
}

static int parent_exists_and_is_dir(const char *path) {
    char parent[WILDNIX_VFS_MAX_NAME];

    parent_path(path, parent, sizeof(parent));

    struct wildnix_vfs_node *node = find_node(parent);

    if (node == 0) {
        return 0;
    }

    return node->type == WILDNIX_VFS_DIR;
}

void wildnix_vfs_init(void) {
    for (size_t i = 0; i < WILDNIX_VFS_MAX_NODES; ++i) {
        nodes[i].used = 0;
        nodes[i].inode = 0;
        nodes[i].size = 0;
        nodes[i].path[0] = '\0';
    }

    next_inode = 1;

    struct wildnix_vfs_node *root = alloc_node();

    if (root != 0) {
        root->type = WILDNIX_VFS_DIR;
        string_copy(root->path, "/", sizeof(root->path));
    }

    wildnix_vfs_create_dir("/bin");
    wildnix_vfs_create_dir("/etc");
    wildnix_vfs_create_dir("/home");
    wildnix_vfs_create_dir("/tmp");
    wildnix_vfs_create_file("/etc/os-release");
    wildnix_vfs_write_file(
        "/etc/os-release",
        (const uint8_t *)"NAME=WildNIX Next\nID=unix-like\nHOME_URL=https://github.com/wildnix/wildnix-next", sizeof("NAME=WildNIX Next\nID=unix-like\nHOME_URL=https://github.com/wildnix/wildnix-next"));
    wildnix_vfs_create_file("/hello.txt");
    wildnix_vfs_write_file(
        "/hello.txt",
        (const uint8_t *)"hello from vfs\n",
        15
    );
}

int wildnix_vfs_exists(const char *path) {
    return find_node(path) != 0;
}

int wildnix_vfs_is_dir(const char *path) {
    struct wildnix_vfs_node *node = find_node(path);

    if (node == 0) {
        return 0;
    }

    return node->type == WILDNIX_VFS_DIR;
}

int wildnix_vfs_is_file(const char *path) {
    struct wildnix_vfs_node *node = find_node(path);

    if (node == 0) {
        return 0;
    }

    return node->type == WILDNIX_VFS_FILE;
}

int wildnix_vfs_create_file(const char *path) {
    if (!valid_path(path)) {
        return 0;
    }

    if (string_equal(path, "/")) {
        return 0;
    }

    if (find_node(path) != 0) {
        return 0;
    }

    if (!parent_exists_and_is_dir(path)) {
        return 0;
    }

    struct wildnix_vfs_node *node = alloc_node();

    if (node == 0) {
        return 0;
    }

    node->type = WILDNIX_VFS_FILE;
    node->size = 0;
    string_copy(node->path, path, sizeof(node->path));

    return 1;
}

int wildnix_vfs_create_dir(const char *path) {
    if (!valid_path(path)) {
        return 0;
    }

    if (string_equal(path, "/")) {
        return 0;
    }

    if (find_node(path) != 0) {
        return 0;
    }

    if (!parent_exists_and_is_dir(path)) {
        return 0;
    }

    struct wildnix_vfs_node *node = alloc_node();

    if (node == 0) {
        return 0;
    }

    node->type = WILDNIX_VFS_DIR;
    node->size = 0;
    string_copy(node->path, path, sizeof(node->path));

    return 1;
}

int wildnix_vfs_delete(const char *path) {
    if (!valid_path(path)) {
        return 0;
    }

    if (string_equal(path, "/")) {
        return 0;
    }

    struct wildnix_vfs_node *node = find_node(path);

    if (node == 0) {
        return 0;
    }

    /*
     * Nie usuwaj niepustych katalogów.
     */
    if (node->type == WILDNIX_VFS_DIR) {
        size_t prefix_len = string_len(path);

        for (size_t i = 0; i < WILDNIX_VFS_MAX_NODES; ++i) {
            if (!nodes[i].used) {
                continue;
            }

            if (&nodes[i] == node) {
                continue;
            }

            if (string_starts_with(nodes[i].path, path) &&
                nodes[i].path[prefix_len] == '/') {
                return 0;
            }
        }
    }

    node->used = 0;
    return 1;
}

int wildnix_vfs_write_file(const char *path, const uint8_t *data, size_t size) {
    if (!valid_path(path) || data == 0) {
        return 0;
    }

    struct wildnix_vfs_node *node = find_node(path);

    if (node == 0) {
        return 0;
    }

    if (node->type != WILDNIX_VFS_FILE) {
        return 0;
    }

    if (size > WILDNIX_VFS_MAX_DATA) {
        size = WILDNIX_VFS_MAX_DATA;
    }

    for (size_t i = 0; i < size; ++i) {
        node->data[i] = data[i];
    }

    node->size = size;

    return 1;
}

int wildnix_vfs_append_file(const char *path, const uint8_t *data, size_t size) {
    if (!valid_path(path) || data == 0) {
        return 0;
    }

    struct wildnix_vfs_node *node = find_node(path);

    if (node == 0) {
        return 0;
    }

    if (node->type != WILDNIX_VFS_FILE) {
        return 0;
    }

    size_t available = WILDNIX_VFS_MAX_DATA - node->size;

    if (size > available) {
        size = available;
    }

    for (size_t i = 0; i < size; ++i) {
        node->data[node->size + i] = data[i];
    }

    node->size += size;

    return 1;
}

const uint8_t *wildnix_vfs_read_file(const char *path, size_t *out_size) {
    if (out_size != 0) {
        *out_size = 0;
    }

    if (!valid_path(path)) {
        return 0;
    }

    struct wildnix_vfs_node *node = find_node(path);

    if (node == 0) {
        return 0;
    }

    if (node->type != WILDNIX_VFS_FILE) {
        return 0;
    }

    if (out_size != 0) {
        *out_size = node->size;
    }

    return node->data;
}

void wildnix_vfs_list_dir(const char *path) {
    if (!valid_path(path)) {
        wildnix_console_write("invalid path\n");
        return;
    }

    struct wildnix_vfs_node *dir = find_node(path);

    if (dir == 0) {
        wildnix_console_write("not found\n");
        return;
    }

    if (dir->type != WILDNIX_VFS_DIR) {
        wildnix_console_write("not a directory\n");
        return;
    }

    size_t path_len = string_len(path);

    for (size_t i = 0; i < WILDNIX_VFS_MAX_NODES; ++i) {
        if (!nodes[i].used) {
            continue;
        }

        if (string_equal(nodes[i].path, path)) {
            continue;
        }

        const char *node_path = nodes[i].path;

        if (string_equal(path, "/")) {
            if (node_path[0] != '/') {
                continue;
            }

            const char *rest = node_path + 1;

            if (rest[0] == '\0') {
                continue;
            }

            int direct_child = 1;

            for (size_t j = 0; rest[j] != '\0'; ++j) {
                if (rest[j] == '/') {
                    direct_child = 0;
                    break;
                }
            }

            if (!direct_child) {
                continue;
            }

            wildnix_console_write(rest);
        } else {
            if (!string_starts_with(node_path, path)) {
                continue;
            }

            if (node_path[path_len] != '/') {
                continue;
            }

            const char *rest = node_path + path_len + 1;

            if (rest[0] == '\0') {
                continue;
            }

            int direct_child = 1;

            for (size_t j = 0; rest[j] != '\0'; ++j) {
                if (rest[j] == '/') {
                    direct_child = 0;
                    break;
                }
            }

            if (!direct_child) {
                continue;
            }

            wildnix_console_write(rest);
        }

        if (nodes[i].type == WILDNIX_VFS_DIR) {
            wildnix_console_write("/");
        }

        wildnix_console_putchar('\n');
    }
}
