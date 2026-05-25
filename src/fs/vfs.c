#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wnu/console.h"
#include "wnu/vfs.h"

static struct wnu_vfs_node nodes[WNU_VFS_MAX_NODES];
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

static struct wnu_vfs_node *find_node(const char *path) {
	for (size_t i = 0; i < WNU_VFS_MAX_NODES; ++i) {
		if (!nodes[i].used) {
			continue;
		}

		if (string_equal(nodes[i].path, path)) {
			return &nodes[i];
		}
	}

	return 0;
}

static struct wnu_vfs_node *alloc_node(void) {
	for (size_t i = 0; i < WNU_VFS_MAX_NODES; ++i) {
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

	if (string_len(path) >= WNU_VFS_MAX_NAME) {
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
	char parent[WNU_VFS_MAX_NAME];

	parent_path(path, parent, sizeof(parent));

	struct wnu_vfs_node *node = find_node(parent);

	if (node == 0) {
		return 0;
	}

	return node->type == WNU_VFS_DIR;
}

static const char *path_basename(const char *path) {
	size_t len = string_len(path);

	if (len == 0 || string_equal(path, "/")) {
		return "/";
	}

	size_t last_slash = 0;

	for (size_t i = 0; i < len; ++i) {
		if (path[i] == '/') {
			last_slash = i;
		}
	}

	return path + last_slash + 1;
}

static int is_direct_child(const char *parent, const char *child) {
	if (string_equal(parent, child)) {
		return 0;
	}

	if (string_equal(parent, "/")) {
		if (child[0] != '/') {
			return 0;
		}

		const char *rest = child + 1;

		if (rest[0] == '\0') {
			return 0;
		}

		for (size_t i = 0; rest[i] != '\0'; ++i) {
			if (rest[i] == '/') {
				return 0;
			}
		}

		return 1;
	}

	size_t parent_len = string_len(parent);

	if (!string_starts_with(child, parent)) {
		return 0;
	}

	if (child[parent_len] != '/') {
		return 0;
	}

	const char *rest = child + parent_len + 1;

	if (rest[0] == '\0') {
		return 0;
	}

	for (size_t i = 0; rest[i] != '\0'; ++i) {
		if (rest[i] == '/') {
			return 0;
		}
	}

	return 1;
}

void wnu_vfs_init(void) {
	for (size_t i = 0; i < WNU_VFS_MAX_NODES; ++i) {
		nodes[i].used = 0;
		nodes[i].inode = 0;
		nodes[i].size = 0;
		nodes[i].path[0] = '\0';
	}

	next_inode = 1;

	struct wnu_vfs_node *root = alloc_node();

	if (root != 0) {
		root->type = WNU_VFS_DIR;
		string_copy(root->path, "/", sizeof(root->path));
	}

	wnu_vfs_create_dir("/bin");
	wnu_vfs_create_dir("/etc");
	wnu_vfs_create_dir("/home");
	wnu_vfs_create_dir("/tmp");

	wnu_vfs_create_file("/etc/os-release");

	static const char os_release[] =
		"NAME=WNU\n"
		"ID=unix-like\n"
		"HOME_URL=https://github.com/segfaultuwu/wnu\n";

	wnu_vfs_write_file(
		"/etc/os-release",
		(const uint8_t *)os_release,
		string_len(os_release)
	);

	wnu_vfs_create_file("/hello.txt");
	wnu_vfs_write_file(
		"/hello.txt",
		(const uint8_t *)"hello from vfs\n",
		15
	);
}

int wnu_vfs_exists(const char *path) {
	return find_node(path) != 0;
}

int wnu_vfs_is_dir(const char *path) {
	struct wnu_vfs_node *node = find_node(path);

	if (node == 0) {
		return 0;
	}

	return node->type == WNU_VFS_DIR;
}

int wnu_vfs_is_file(const char *path) {
	struct wnu_vfs_node *node = find_node(path);

	if (node == 0) {
		return 0;
	}

	return node->type == WNU_VFS_FILE;
}

int wnu_vfs_create_file(const char *path) {
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

	struct wnu_vfs_node *node = alloc_node();

	if (node == 0) {
		return 0;
	}

	node->type = WNU_VFS_FILE;
	node->size = 0;
	string_copy(node->path, path, sizeof(node->path));

	return 1;
}

int wnu_vfs_create_dir(const char *path) {
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

	struct wnu_vfs_node *node = alloc_node();

	if (node == 0) {
		return 0;
	}

	node->type = WNU_VFS_DIR;
	node->size = 0;
	string_copy(node->path, path, sizeof(node->path));

	return 1;
}

int wnu_vfs_delete(const char *path) {
	if (!valid_path(path)) {
		return 0;
	}

	if (string_equal(path, "/")) {
		return 0;
	}

	struct wnu_vfs_node *node = find_node(path);

	if (node == 0) {
		return 0;
	}

	/*
	 * Do not delete non-empty directories.
	 */
	if (node->type == WNU_VFS_DIR) {
		size_t prefix_len = string_len(path);

		for (size_t i = 0; i < WNU_VFS_MAX_NODES; ++i) {
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

int wnu_vfs_write_file(const char *path, const uint8_t *data, size_t size) {
	if (!valid_path(path) || data == 0) {
		return 0;
	}

	struct wnu_vfs_node *node = find_node(path);

	if (node == 0) {
		return 0;
	}

	if (node->type != WNU_VFS_FILE) {
		return 0;
	}

	if (size > WNU_VFS_MAX_DATA) {
		size = WNU_VFS_MAX_DATA;
	}

	for (size_t i = 0; i < size; ++i) {
		node->data[i] = data[i];
	}

	node->size = size;

	return 1;
}

int wnu_vfs_append_file(const char *path, const uint8_t *data, size_t size) {
	if (!valid_path(path) || data == 0) {
		return 0;
	}

	struct wnu_vfs_node *node = find_node(path);

	if (node == 0) {
		return 0;
	}

	if (node->type != WNU_VFS_FILE) {
		return 0;
	}

	size_t available = WNU_VFS_MAX_DATA - node->size;

	if (size > available) {
		size = available;
	}

	for (size_t i = 0; i < size; ++i) {
		node->data[node->size + i] = data[i];
	}

	node->size += size;

	return 1;
}

const uint8_t *wnu_vfs_read_file(const char *path, size_t *out_size) {
	if (out_size != 0) {
		*out_size = 0;
	}

	if (!valid_path(path)) {
		return 0;
	}

	struct wnu_vfs_node *node = find_node(path);

	if (node == 0) {
		return 0;
	}

	if (node->type != WNU_VFS_FILE) {
		return 0;
	}

	if (out_size != 0) {
		*out_size = node->size;
	}

	return node->data;
}

void wnu_vfs_list_dir(const char *path) {
	if (!valid_path(path)) {
		wnu_console_write("invalid path\n");
		return;
	}

	struct wnu_vfs_node *dir = find_node(path);

	if (dir == 0) {
		wnu_console_write("not found\n");
		return;
	}

	if (dir->type != WNU_VFS_DIR) {
		wnu_console_write("not a directory\n");
		return;
	}

	for (size_t i = 0; i < WNU_VFS_MAX_NODES; ++i) {
		if (!nodes[i].used) {
			continue;
		}

		if (!is_direct_child(path, nodes[i].path)) {
			continue;
		}

		wnu_console_write(path_basename(nodes[i].path));

		if (nodes[i].type == WNU_VFS_DIR) {
			wnu_console_write("/");
		}

		wnu_console_putchar('\n');
	}
}

static void tree_indent(int depth) {
	for (int i = 0; i < depth; ++i) {
		wnu_console_write("  ");
	}
}

static void tree_walk(const char *path, int depth) {
	struct wnu_vfs_node *node = find_node(path);

	if (node == 0) {
		return;
	}

	tree_indent(depth);

	if (string_equal(path, "/")) {
		wnu_console_write("/");
	} else {
		wnu_console_write(path_basename(node->path));
	}

	if (node->type == WNU_VFS_DIR && !string_equal(path, "/")) {
		wnu_console_write("/");
	}

	wnu_console_putchar('\n');

	if (node->type != WNU_VFS_DIR) {
		return;
	}

	for (size_t i = 0; i < WNU_VFS_MAX_NODES; ++i) {
		if (!nodes[i].used) {
			continue;
		}

		if (!is_direct_child(path, nodes[i].path)) {
			continue;
		}

		tree_walk(nodes[i].path, depth + 1);
	}
}

void wnu_vfs_tree(const char *path) {
	if (!valid_path(path)) {
		wnu_console_write("tree: invalid path\n");
		return;
	}

	if (!wnu_vfs_exists(path)) {
		wnu_console_write("tree: not found\n");
		return;
	}

	tree_walk(path, 0);
}
