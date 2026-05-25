#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "wnu/block.h"
#include "wnu/vfs.h"
#include "wnu/console.h"
#include "wnu/fat32.h"

/* Minimal FAT32 mount: read boot sector, read root cluster, create files under mountpoint */

static uint16_t read_u16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t read_u32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

int fat32_mount(const char *devname, const char *mountpoint) {
    if (!wnu_block_find(devname)) {
        wnu_console_write("fat32: device not found\n");
        return 0;
    }

    uint8_t sector[512];
    if (!wnu_block_read(devname, 0, sector, 1)) {
        wnu_console_write("fat32: failed to read boot sector\n");
        return 0;
    }

    uint16_t bytes_per_sector = read_u16(&sector[11]);
    uint8_t sectors_per_cluster = sector[13];
    uint16_t reserved_sectors = read_u16(&sector[14]);
    uint8_t num_fats = sector[16];
    uint32_t fat_size = read_u32(&sector[36]);
    uint32_t root_cluster = read_u32(&sector[44]);

    if (bytes_per_sector == 0) {
        wnu_console_write("fat32: invalid bytes per sector\n");
        return 0;
    }

    uint32_t first_fat_sector = reserved_sectors;
    uint32_t first_data_sector = reserved_sectors + (num_fats * fat_size);
    uint32_t sectors_per_cluster_u32 = (uint32_t)sectors_per_cluster;
    uint32_t first_root_sector = first_data_sector + (root_cluster - 2) * sectors_per_cluster_u32;

    /* ensure mountpoint exists */
    if (!wnu_vfs_exists(mountpoint)) {
        if (!wnu_vfs_create_dir(mountpoint)) {
            wnu_console_write("fat32: failed to create mountpoint\n");
            return 0;
        }
    }

    /* Read first cluster of root */
    size_t to_read = sectors_per_cluster_u32;
    uint8_t cluster_buf[8192]; /* support up to 8KB clusters */
    if (bytes_per_sector * sectors_per_cluster_u32 > sizeof(cluster_buf)) {
        wnu_console_write("fat32: cluster too large\n");
        return 0;
    }

    if (!wnu_block_read(devname, first_root_sector, cluster_buf, to_read)) {
        wnu_console_write("fat32: failed to read root cluster\n");
        return 0;
    }

    /* Parse directory entries */
    for (size_t offset = 0; offset + 32 <= bytes_per_sector * sectors_per_cluster_u32; offset += 32) {
        uint8_t first = cluster_buf[offset];
        if (first == 0x00) break; /* end */
        if (first == 0xE5) continue; /* deleted */

        uint8_t attr = cluster_buf[offset + 11];
        if (attr & 0x08) continue; /* volume label */
        if (attr & 0x0F) continue; /* LFN entry or special */

        /* name (8) + ext (3) */
        char name[16];
        size_t npos = 0;
        for (int i = 0; i < 8; ++i) {
            char c = cluster_buf[offset + i];
            if (c == ' ') break;
            name[npos++] = c;
        }
        if (cluster_buf[offset + 8] != ' ') {
            name[npos++] = '.';
            for (int i = 0; i < 3; ++i) {
                char c = cluster_buf[offset + 8 + i];
                if (c == ' ') break;
                name[npos++] = c;
            }
        }
        name[npos] = '\0';

        uint32_t high = read_u16(&cluster_buf[offset + 20]);
        uint32_t low = read_u16(&cluster_buf[offset + 26]);
        uint32_t start_cluster = (high << 16) | low;
        uint32_t filesize = read_u32(&cluster_buf[offset + 28]);

        /* Read file contents (simple: assume contiguous) */
        if (filesize == 0) {
            /* create empty file */
            char path[WNU_VFS_MAX_NAME];
            size_t p = 0;
            /* mountpoint + '/' + name */
            for (size_t i = 0; mountpoint[i] != '\0' && p + 1 < sizeof(path); ++i) path[p++] = mountpoint[i];
            if (p + 1 < sizeof(path)) path[p++] = '/';
            for (size_t i = 0; name[i] != '\0' && p + 1 < sizeof(path); ++i) path[p++] = name[i];
            path[p] = '\0';
            wnu_vfs_create_file(path);
            continue;
        }

        uint32_t cluster = start_cluster;
        uint32_t bytes_left = filesize;
        uint8_t file_buf[WNU_VFS_MAX_DATA];
        size_t written = 0;

        /* helper: read FAT entry for a cluster */

        uint8_t fat_sector_buf[4096];
        if (bytes_per_sector > sizeof(fat_sector_buf)) {
            wnu_console_write("fat32: unsupported sector size for FAT read\n");
            continue;
        }

        while (bytes_left > 0 && written < sizeof(file_buf)) {
            uint32_t sector_of_cluster = first_data_sector + (cluster - 2) * sectors_per_cluster_u32;
            uint32_t sectors_to_read = sectors_per_cluster_u32;
            if (!wnu_block_read(devname, sector_of_cluster, cluster_buf, sectors_to_read)) {
                wnu_console_write("fat32: failed to read file cluster\n");
                break;
            }

            uint32_t copy = bytes_per_sector * sectors_per_cluster_u32;
            if (copy > bytes_left) copy = bytes_left;
            if (copy > sizeof(file_buf) - written) copy = sizeof(file_buf) - written;
            for (uint32_t i = 0; i < copy; ++i) file_buf[written + i] = cluster_buf[i];
            written += copy;
            bytes_left -= copy;

            /* Read next cluster from FAT */
            uint64_t fat_offset = (uint64_t)cluster * 4;
            uint32_t fat_sector = first_fat_sector + (uint32_t)(fat_offset / bytes_per_sector);
            uint32_t fat_sector_offset = (uint32_t)(fat_offset % bytes_per_sector);

            if (!wnu_block_read(devname, fat_sector, fat_sector_buf, 1)) {
                wnu_console_write("fat32: failed to read FAT sector\n");
                break;
            }

            uint32_t entry = (uint32_t)fat_sector_buf[fat_sector_offset] |
                             ((uint32_t)fat_sector_buf[fat_sector_offset + 1] << 8) |
                             ((uint32_t)fat_sector_buf[fat_sector_offset + 2] << 16) |
                             ((uint32_t)fat_sector_buf[fat_sector_offset + 3] << 24);

            entry &= 0x0FFFFFFFu;

            if (entry >= 0x0FFFFFF8u) {
                /* end of chain */
                break;
            }

            if (entry == 0x00000000u) {
                /* free cluster? stop */
                break;
            }

            cluster = entry;
        }

        /* write into VFS */
        char path[WNU_VFS_MAX_NAME];
        size_t p = 0;
        for (size_t i = 0; mountpoint[i] != '\0' && p + 1 < sizeof(path); ++i) path[p++] = mountpoint[i];
        if (p + 1 < sizeof(path)) path[p++] = '/';
        for (size_t i = 0; name[i] != '\0' && p + 1 < sizeof(path); ++i) path[p++] = name[i];
        path[p] = '\0';

        if (!wnu_vfs_create_file(path)) {
            wnu_console_write("fat32: failed to create file in VFS\n");
            continue;
        }

        if (!wnu_vfs_write_file(path, file_buf, written)) {
            wnu_console_write("fat32: failed to write file in VFS\n");
            continue;
        }
    }

    wnu_console_write("fat32: mounted\n");
    return 1;
}
