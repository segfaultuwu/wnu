#include <limine.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wnu/arch.h"
#include "wnu/console.h"
#include "wnu/keyboard.h"
#include "wnu/platform.h"
#include "wnu/config.h"
#include "wnu/vfs.h"

#define PATH_MAX 64

__attribute__((used, section(".limine_reqs_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_reqs")))
static volatile LIMINE_BASE_REVISION(0);

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

static char current_dir[PATH_MAX] = "/";

static void panic(void) {
    while (true) {
        wnu_halt();
    }
}

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

/*
 * Resolve path:
 *
 * "/bin"    -> "/bin"
 * "file"    -> "/current/file"
 * "."       -> current_dir
 *
 */
static int resolve_path(const char *input, char *out, size_t out_size) {
    if (input == 0 || out == 0 || out_size == 0) {
        return 0;
    }

    if (input[0] == '\0') {
        return 0;
    }

    if (string_equal(input, ".")) {
        string_copy(out, current_dir, out_size);
        return 1;
    }

    /*
     * Absolute path.
     */
    if (input[0] == '/') {
        if (string_len(input) >= out_size) {
            return 0;
        }

        string_copy(out, input, out_size);
        return 1;
    }

    /*
     * Relative path.
     */
    size_t pos = 0;

    if (string_equal(current_dir, "/")) {
        if (pos + 1 >= out_size) {
            return 0;
        }

        out[pos++] = '/';
    } else {
        size_t base_len = string_len(current_dir);

        if (base_len + 1 >= out_size) {
            return 0;
        }

        for (size_t i = 0; i < base_len; ++i) {
            out[pos++] = current_dir[i];
        }

        if (pos + 1 >= out_size) {
            return 0;
        }

        out[pos++] = '/';
    }

    for (size_t i = 0; input[i] != '\0'; ++i) {
        if (pos + 1 >= out_size) {
            return 0;
        }

        out[pos++] = input[i];
    }

    out[pos] = '\0';
    return 1;
}

static void print_prompt(void) {
    wnu_console_write(current_dir);
    wnu_console_write(" % ");
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

static void shell_execute(const char *line) {
    if (string_equal(line, "")) {
        return;
    }

    if (string_equal(line, "help")) {
        wnu_console_write("commands:\n");
        wnu_console_write("  help\n");
        wnu_console_write("  clear\n");
        wnu_console_write("  about\n");
        wnu_console_write("  echo <text>\n");
        wnu_console_write("  pwd\n");
        wnu_console_write("  cd <path>\n");
        wnu_console_write("  ls [path]\n");
        wnu_console_write("  cat <path>\n");
        wnu_console_write("  touch <path>\n");
        wnu_console_write("  mkdir <path>\n");
        wnu_console_write("  rm <path>\n");
        wnu_console_write("  write <path> <text>\n");
        return;
    }

    if (string_equal(line, "clear")) {
        wnu_console_clear();
        return;
    }

    if (string_equal(line, "about")) {
        wnu_console_write("WNU v");
        wnu_console_write(WNU_VERSION);
        wnu_console_putchar('\n');
        return;
    }

    if (string_equal(line, "pwd")) {
        wnu_console_write(current_dir);
        wnu_console_putchar('\n');
        return;
    }

    if (string_starts_with(line, "echo ")) {
        wnu_console_write(line + 5);
        wnu_console_putchar('\n');
        return;
    }

    if (string_starts_with(line, "cd ")) {
        char path[PATH_MAX];

        if (!resolve_path(line + 3, path, PATH_MAX)) {
            wnu_console_write("cd: invalid path\n");
            return;
        }

        if (!wnu_vfs_exists(path)) {
            wnu_console_write("cd: no such file or directory\n");
            return;
        }

        if (!wnu_vfs_is_dir(path)) {
            wnu_console_write("cd: not a directory\n");
            return;
        }

        string_copy(current_dir, path, PATH_MAX);
        return;
    }

    if (string_equal(line, "ls")) {
        wnu_vfs_list_dir(current_dir);
        return;
    }

    if (string_starts_with(line, "ls ")) {
        char path[PATH_MAX];

        if (!resolve_path(line + 3, path, PATH_MAX)) {
            wnu_console_write("ls: invalid path\n");
            return;
        }

        wnu_vfs_list_dir(path);
        return;
    }

    if (string_starts_with(line, "cat ")) {
        char path[PATH_MAX];

        if (!resolve_path(line + 4, path, PATH_MAX)) {
            wnu_console_write("cat: invalid path\n");
            return;
        }

        size_t size = 0;
        const uint8_t *data = wnu_vfs_read_file(path, &size);

        if (data == 0) {
            wnu_console_write("cat: cannot read file\n");
            return;
        }

        wnu_console_write_len((const char *)data, size);

        if (size == 0 || data[size - 1] != '\n') {
            wnu_console_putchar('\n');
        }

        return;
    }

    if (string_starts_with(line, "touch ")) {
        char path[PATH_MAX];

        if (!resolve_path(line + 6, path, PATH_MAX)) {
            wnu_console_write("touch: invalid path\n");
            return;
        }

        if (!wnu_vfs_create_file(path)) {
            wnu_console_write("touch: failed\n");
        }

        return;
    }

    if (string_starts_with(line, "mkdir ")) {
        char path[PATH_MAX];

        if (!resolve_path(line + 6, path, PATH_MAX)) {
            wnu_console_write("mkdir: invalid path\n");
            return;
        }

        if (!wnu_vfs_create_dir(path)) {
            wnu_console_write("mkdir: failed\n");
        }

        return;
    }

    if (string_starts_with(line, "rm ")) {
        char path[PATH_MAX];

        if (!resolve_path(line + 3, path, PATH_MAX)) {
            wnu_console_write("rm: invalid path\n");
            return;
        }

        if (!wnu_vfs_delete(path)) {
            wnu_console_write("rm: failed\n");
        }

        return;
    }

    if (string_starts_with(line, "write ")) {
        /*
         * Format:
         * write /file text here
         * write file text here
         */
        const char *args = line + 6;

        size_t i = 0;
        while (args[i] != '\0' && args[i] != ' ') {
            i++;
        }

        if (args[i] != ' ') {
            wnu_console_write("usage: write /file text\n");
            return;
        }

        char raw_path[PATH_MAX];

        if (i >= PATH_MAX) {
            wnu_console_write("path too long\n");
            return;
        }

        for (size_t j = 0; j < i; ++j) {
            raw_path[j] = args[j];
        }

        raw_path[i] = '\0';

        char path[PATH_MAX];

        if (!resolve_path(raw_path, path, PATH_MAX)) {
            wnu_console_write("write: invalid path\n");
            return;
        }

        const char *text = args + i + 1;

        if (!wnu_vfs_exists(path)) {
            wnu_vfs_create_file(path);
        }

        if (!wnu_vfs_write_file(path, (const uint8_t *)text, string_len(text))) {
            wnu_console_write("write: failed\n");
        }

        return;
    }

    wnu_console_write("unknown command: ");
    wnu_console_write(line);
    wnu_console_putchar('\n');
}

void _start(void) {
    wnu_cli();

    if (!LIMINE_BASE_REVISION_SUPPORTED) {
        panic();
    }

    framebuffer_init();
    font_init();

    wnu_console_bind(&framebuffer, &font);
    wnu_console_clear();

    wnu_vfs_init();

    wnu_console_write("WNU console ready\n");
    wnu_console_write("type 'help'\n");
    print_prompt();

    wnu_keyboard_init();
    wnu_arch_init();
    wnu_sti();

    char line[256];

    while (true) {
        if (wnu_console_line_ready()) {
            wnu_console_readline(line, sizeof(line));

            shell_execute(line);

            print_prompt();
        }

        wnu_halt();
    }
}
