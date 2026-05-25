#include "wnu/elf.h"
#include "wnu/platform.h"

void elf_exec(const uint8_t *data, size_t size) {
    uint64_t entry_point;
    void *loaded = elf_load(data, size, &entry_point);

    if (loaded == 0) {
        return;
    }

    void (*entry)(void) = (void (*)(void))entry_point;
    entry();
}

void *elf_load(const uint8_t *data, size_t size, uint64_t *entry_point) {
    if (size < sizeof(struct ElfHeader)) {
        return 0;
    }

    const struct ElfHeader *header = (const struct ElfHeader *)data;

    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        return 0;
    }

    if (header->e_type != 2 || header->e_machine != 0x3E || header->e_version != 1) {
        return 0;
    }

    if (entry_point != 0) {
        *entry_point = header->e_entry;
    }

    return (void *)header->e_entry;
}