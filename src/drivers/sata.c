#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "wnu/console.h"
#include "wnu/pci.h"
#include "wnu/platform.h"
#include "wnu/block.h"
#include "wnu/sata.h"

#define SATA_MAX_CONTROLLERS 16

#define PCI_CLASS_MASS_STORAGE 0x01
#define PCI_SUBCLASS_SATA      0x06
#define PCI_PROGIF_AHCI        0x01

#define AHCI_BAR_INDEX 5

#define AHCI_REG_CAP 0x00
#define AHCI_REG_PI  0x0C

#define AHCI_PORT_BASE 0x100
#define AHCI_PORT_SIZE 0x80
#define AHCI_PORT_SIG  0x24
#define AHCI_PORT_SSTS 0x28

struct sata_ctrl_info {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;

    uint32_t bar[6];
};

static struct sata_ctrl_info sata_ctrls[SATA_MAX_CONTROLLERS];
static size_t sata_ctrl_count;

static struct wnu_block_device sata_block_devs[SATA_MAX_CONTROLLERS];

/* AHCI helper defs */
#define AHCI_PORT_CLB   0x00
#define AHCI_PORT_CLBU  0x04
#define AHCI_PORT_FB    0x08
#define AHCI_PORT_FBU   0x0C
#define AHCI_PORT_IS    0x10
#define AHCI_PORT_IE    0x14
#define AHCI_PORT_CMD   0x18
#define AHCI_PORT_TFD   0x20
#define AHCI_PORT_SIG   0x24
#define AHCI_PORT_SSTS  0x28
#define AHCI_PORT_SCTL  0x2C
#define AHCI_PORT_SERR  0x30
#define AHCI_PORT_SACT  0x34
#define AHCI_PORT_CI    0x38

#define AHCI_PxCMD_ST   (1u << 0)
#define AHCI_PxCMD_FRE  (1u << 4)
#define AHCI_PxCMD_FR   (1u << 14)
#define AHCI_PxCMD_CR   (1u << 15)

/* Static per-controller buffers (aligned) */
static uint8_t sata_cmd_list[SATA_MAX_CONTROLLERS][1024] __attribute__((aligned(1024)));
static uint8_t sata_fis_recv[SATA_MAX_CONTROLLERS][256] __attribute__((aligned(256)));
static uint8_t sata_cmd_table[SATA_MAX_CONTROLLERS][256] __attribute__((aligned(256)));

static uint32_t pci_read_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar_index) {
    return pci_config_read32(bus, slot, func, (uint8_t)(0x10 + bar_index * 4));
}

static uintptr_t phys_to_virt(uintptr_t phys) {
    return phys - (uintptr_t)wnu_kernel_phys_base +
           (uintptr_t)wnu_kernel_virt_base;
}

static uint32_t mmio_read32(uintptr_t base, uintptr_t offset) {
    volatile uint32_t *ptr = (volatile uint32_t *)(base + offset);
    return *ptr;
}

static void sata_scan_pci(void) {
    sata_ctrl_count = 0;

    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            uint16_t vendor = pci_config_read16((uint8_t)bus, slot, 0, 0x00);

            if (vendor == 0xffff) {
                continue;
            }

            uint8_t header_type =
                pci_config_read8((uint8_t)bus, slot, 0, 0x0E);

            uint8_t func_count = (header_type & 0x80) ? 8 : 1;

            for (uint8_t func = 0; func < func_count; ++func) {
                uint16_t vendor_id =
                    pci_config_read16((uint8_t)bus, slot, func, 0x00);

                if (vendor_id == 0xffff) {
                    continue;
                }

                uint16_t device_id =
                    pci_config_read16((uint8_t)bus, slot, func, 0x02);

                uint8_t class_code =
                    pci_config_read8((uint8_t)bus, slot, func, 0x0B);

                uint8_t subclass =
                    pci_config_read8((uint8_t)bus, slot, func, 0x0A);

                uint8_t prog_if =
                    pci_config_read8((uint8_t)bus, slot, func, 0x09);

                if (class_code != PCI_CLASS_MASS_STORAGE ||
                    subclass != PCI_SUBCLASS_SATA) {
                    continue;
                }

                if (sata_ctrl_count >= SATA_MAX_CONTROLLERS) {
                    printf("sata: controller table full\n");
                    return;
                }

                struct sata_ctrl_info *ctrl = &sata_ctrls[sata_ctrl_count++];

                ctrl->bus = (uint8_t)bus;
                ctrl->slot = slot;
                ctrl->func = func;
                ctrl->vendor_id = vendor_id;
                ctrl->device_id = device_id;
                ctrl->class_code = class_code;
                ctrl->subclass = subclass;
                ctrl->prog_if = prog_if;

                for (int i = 0; i < 6; ++i) {
                    ctrl->bar[i] =
                        pci_read_bar((uint8_t)bus, slot, func, i);
                }
            }
        }
    }
}

static void sata_print_controller(const struct sata_ctrl_info *ctrl, size_t index) {
    printf(
        "sata%u: bus=%u slot=%u func=%u vendor=0x%x device=0x%x prog_if=0x%x\n",
        (unsigned)index,
        (unsigned)ctrl->bus,
        (unsigned)ctrl->slot,
        (unsigned)ctrl->func,
        (unsigned)ctrl->vendor_id,
        (unsigned)ctrl->device_id,
        (unsigned)ctrl->prog_if
    );

    for (int i = 0; i < 6; ++i) {
        printf("  BAR%u = 0x%x\n", (unsigned)i, ctrl->bar[i]);
    }
}

static void sata_probe_ahci_controller(const struct sata_ctrl_info *ctrl, size_t index) {
    if (ctrl->prog_if != PCI_PROGIF_AHCI) {
        printf("sata%u: not AHCI, prog_if=0x%x\n",
            (unsigned)index,
            (unsigned)ctrl->prog_if
        );
        return;
    }

    uint32_t bar5 = ctrl->bar[AHCI_BAR_INDEX];

    if (bar5 == 0) {
        printf("sata%u: missing AHCI BAR5\n", (unsigned)index);
        return;
    }

    if ((bar5 & 1u) != 0) {
        printf("sata%u: BAR5 is I/O space, expected MMIO\n",
            (unsigned)index
        );
        return;
    }

    uintptr_t abar_phys = (uintptr_t)(bar5 & 0xfffffff0u);
    uintptr_t abar_virt = phys_to_virt(abar_phys);

    uint32_t cap = mmio_read32(abar_virt, AHCI_REG_CAP);
    uint32_t pi = mmio_read32(abar_virt, AHCI_REG_PI);

    printf(
        "sata%u: AHCI abar_phys=0x%x cap=0x%x pi=0x%x\n",
        (unsigned)index,
        (unsigned)abar_phys,
        cap,
        pi
    );

    for (int port = 0; port < 32; ++port) {
        if ((pi & (1u << port)) == 0) {
            continue;
        }

        uintptr_t port_base =
            abar_virt + AHCI_PORT_BASE + (uintptr_t)port * AHCI_PORT_SIZE;

        uint32_t sig = mmio_read32(port_base, AHCI_PORT_SIG);
        uint32_t ssts = mmio_read32(port_base, AHCI_PORT_SSTS);

        printf(
            "  port %u: sig=0x%x ssts=0x%x\n",
            (unsigned)port,
            sig,
            ssts
        );
    }
}

static int sata_block_read(void *driver_data, uint64_t lba, void *buf, size_t count) {
    struct sata_ctrl_info *ctrl = (struct sata_ctrl_info *)driver_data;
    if (ctrl == NULL || buf == NULL || count == 0) return 0;

    /* locate controller index */
    int ctrl_idx = -1;
    for (size_t i = 0; i < sata_ctrl_count; ++i) {
        if (&sata_ctrls[i] == ctrl) { ctrl_idx = (int)i; break; }
    }
    if (ctrl_idx < 0) return 0;

    uint32_t bar5 = ctrl->bar[AHCI_BAR_INDEX];
    if (bar5 == 0 || (bar5 & 1u) != 0) return 0;

    uintptr_t abar_phys = (uintptr_t)(bar5 & 0xfffffff0u);
    uintptr_t abar_virt = phys_to_virt(abar_phys);

    uint32_t pi = mmio_read32(abar_virt, AHCI_REG_PI);
    printf("sata_block_read: ctrl_idx=%d pi=0x%x lba=%llu count=%zu\n", ctrl_idx, pi, (unsigned long long)lba, count);

    /* find first active port */
    int port = -1;
    for (int p = 0; p < 32; ++p) {
        if (pi & (1u << p)) { port = p; break; }
    }
    printf("sata_block_read: selected port=%d\n", port);
    if (port < 0) return 0;

    uintptr_t port_base = abar_virt + AHCI_PORT_BASE + (uintptr_t)port * AHCI_PORT_SIZE;

    /* Prepare command list / FIS receive / command table for this controller index */
    uintptr_t clb_phys = wnu_virt_to_phys(&sata_cmd_list[ctrl_idx][0]);
    uintptr_t fb_phys = wnu_virt_to_phys(&sata_fis_recv[ctrl_idx][0]);
    uintptr_t ctba_phys = wnu_virt_to_phys(&sata_cmd_table[ctrl_idx][0]);

    /* Write CLB and FB registers */
    *((volatile uint32_t *)(port_base + AHCI_PORT_CLB)) = (uint32_t)(clb_phys & 0xffffffffu);
    *((volatile uint32_t *)(port_base + AHCI_PORT_CLBU)) = (uint32_t)((clb_phys >> 32) & 0xffffffffu);
    *((volatile uint32_t *)(port_base + AHCI_PORT_FB)) = (uint32_t)(fb_phys & 0xffffffffu);
    *((volatile uint32_t *)(port_base + AHCI_PORT_FBU)) = (uint32_t)((fb_phys >> 32) & 0xffffffffu);

    /* Clear interrupt status */
    *((volatile uint32_t *)(port_base + AHCI_PORT_IS)) = 0xffffffffu;
    printf("sata_block_read: wrote CLB/FB, clb_phys=0x%x fb_phys=0x%x ctba=0x%x\n", (unsigned)clb_phys, (unsigned)fb_phys, (unsigned)ctba_phys);

    /* Start command engine: set FRE and ST */
    uint32_t cmd = mmio_read32(port_base, AHCI_PORT_CMD);
    cmd |= AHCI_PxCMD_FRE;
    mmio_read32(port_base, AHCI_PORT_CMD); /* dummy read */
    mmio_read32(port_base, AHCI_PORT_CMD);
    volatile uint32_t *pcmd = (volatile uint32_t *)(port_base + AHCI_PORT_CMD);
    *pcmd = cmd | AHCI_PxCMD_ST;

    /* Build command header (slot 0) */
    memset(&sata_cmd_list[ctrl_idx][0], 0, 1024);

    /* Command header at offset 0 */
    uint8_t *ch = &sata_cmd_list[ctrl_idx][0];
    ch[0] = 5; /* cfl = 5 (20 bytes) */
    ch[1] = 0; /* prdtl in byte 2 - we'll set at offset 2 */
    ch[2] = 0;
    ch[3] = 0;
    ch[4] = 1; /* PRDTL = 1 */

    /* Set command table physical addr in header DW4/DW5 (offset 0x18) */
    uint64_t ctba = ctba_phys;
    *((uint32_t *)&ch[0x18]) = (uint32_t)(ctba & 0xffffffffu);
    *((uint32_t *)&ch[0x1C]) = (uint32_t)((ctba >> 32) & 0xffffffffu);

    /* Build command table */
    memset(&sata_cmd_table[ctrl_idx][0], 0, 256);
    uint8_t *ct = &sata_cmd_table[ctrl_idx][0];

    /* CFIS at offset 0 */
    uint8_t *cfis = &ct[0];
    cfis[0] = 0x27; /* FIS_TYPE_REG_H2D */
    cfis[1] = 1u << 7; /* command */
    cfis[2] = 0x25; /* READ DMA EXT */

    /* 48-bit LBA */
    cfis[4] = (uint8_t)(lba & 0xFF);
    cfis[5] = (uint8_t)((lba >> 8) & 0xFF);
    cfis[6] = (uint8_t)((lba >> 16) & 0xFF);
    /* device register left as 0 */
    cfis[8] = (uint8_t)((lba >> 24) & 0xFF);
    cfis[9] = (uint8_t)((lba >> 32) & 0xFF);
    cfis[10] = (uint8_t)((lba >> 40) & 0xFF);

    /* sector count */
    cfis[12] = (uint8_t)(count & 0xFF);
    cfis[13] = (uint8_t)((count >> 8) & 0xFF);

    /* PRDT entry at offset 0x80 */
    uint32_t *prdt = (uint32_t *)&ct[0x80];
    uint64_t buf_phys = wnu_virt_to_phys(buf);
    prdt[0] = (uint32_t)(buf_phys & 0xffffffffu);
    prdt[1] = (uint32_t)((buf_phys >> 32) & 0xffffffffu);
    /* dbc: byte count -1, set interrupt on completion (bit 31) */
    uint32_t dbc = (uint32_t)(count * 512 - 1);
    prdt[3] = dbc | 0x80000000u;

    /* Issue command: set CI bit 0 */
    uint32_t ci = mmio_read32(port_base, AHCI_PORT_CI);
    ci |= 1u;
    mmio_read32(port_base, AHCI_PORT_CI);
    *((volatile uint32_t *)(port_base + AHCI_PORT_CI)) = ci;
    printf("sata_block_read: issued CI, waiting for completion\n");

    /* Poll for completion */
    for (int t = 0; t < 1000000; ++t) {
        uint32_t cur = mmio_read32(port_base, AHCI_PORT_CI);
        if ((cur & 1u) == 0) break;
    }

    /* Check for errors in TFD */
    uint32_t tfd = mmio_read32(port_base, AHCI_PORT_TFD);
    printf("sata_block_read: done, tfd=0x%x\n", tfd);
    if (tfd & 0x1) {
        printf("sata: tfd error 0x%x\n", tfd);
        return 0;
    }

    return (int)count;
}

void wnu_sata_init(void) {
    printf("sata: init\n");

    sata_scan_pci();

    if (sata_ctrl_count == 0) {
        printf("sata: no SATA controllers found\n");
        return;
    }

    for (size_t i = 0; i < sata_ctrl_count; ++i) {
        struct sata_ctrl_info *c = &sata_ctrls[i];
        sata_print_controller(c, i);
        sata_probe_ahci_controller(c, i);

        /* Register a block device placeholder for this controller */
        struct wnu_block_device *dev = &sata_block_devs[i];
        for (size_t k = 0; k < sizeof(dev->name); ++k) dev->name[k] = 0;
        /* format name as "sata<N>" without snprintf */
        dev->name[0] = 's'; dev->name[1] = 'a'; dev->name[2] = 't'; dev->name[3] = 'a';
        unsigned num = (unsigned)i;
        int pos = 4;
        char buf[16];
        int bl = 0;
        if (num == 0) { buf[bl++] = '0'; }
        while (num > 0 && bl < (int)sizeof(buf)-1) { buf[bl++] = '0' + (num % 10); num /= 10; }
        /* reverse */
        for (int j = 0; j < bl && pos < (int)sizeof(dev->name)-1; ++j) {
            dev->name[pos++] = buf[bl - 1 - j];
        }
        dev->name[pos] = '\0';
        dev->driver_data = (void *)c;
        dev->read = sata_block_read; /* temporary stub */
        dev->capacity = 0;
        if (wnu_block_register(dev)) {
            printf("sata: registered block device %s\n", dev->name);
        } else {
            printf("sata: failed to register block device %s\n", dev->name);
        }
    }
}

static const char *ahci_device_type(uint32_t sig) {
    switch (sig) {
        case 0x00000101:
            return "SATA";
        case 0xeb140101:
            return "ATAPI";
        case 0xc33c0101:
            return "SEMB";
        case 0x96690101:
            return "PM";
        default:
            return "UNKNOWN";
    }
}

static const char *ahci_link_state(uint32_t ssts) {
    uint32_t det = ssts & 0x0f;

    switch (det) {
        case 0:
            return "no-device";
        case 1:
            return "present-no-phy";
        case 3:
            return "online";
        case 4:
            return "offline";
        default:
            return "unknown";
    }
}

void wnu_sata_print_devices(void) {
    sata_scan_pci();

    if (sata_ctrl_count == 0) {
        wnu_console_write("NAME   TYPE      STATE       BUS:SLOT.FUNC  INFO\n");
        wnu_console_write("none   -         -           -              no SATA/AHCI controllers found\n");
        return;
    }

    wnu_console_write("NAME   TYPE      STATE       BUS:SLOT.FUNC  INFO\n");

    for (size_t i = 0; i < sata_ctrl_count; ++i) {
        struct sata_ctrl_info *ctrl = &sata_ctrls[i];

        if (ctrl->prog_if != PCI_PROGIF_AHCI) {
            printf(
                "sata%u controller non-ahci  %u:%u.%u       vendor=0x%x device=0x%x\n",
                (unsigned)i,
                (unsigned)ctrl->bus,
                (unsigned)ctrl->slot,
                (unsigned)ctrl->func,
                (unsigned)ctrl->vendor_id,
                (unsigned)ctrl->device_id
            );
            continue;
        }

        uint32_t bar5 = ctrl->bar[AHCI_BAR_INDEX];

        if (bar5 == 0 || (bar5 & 1u) != 0) {
            printf(
                "sata%u controller bad-bar   %u:%u.%u       BAR5=0x%x\n",
                (unsigned)i,
                (unsigned)ctrl->bus,
                (unsigned)ctrl->slot,
                (unsigned)ctrl->func,
                (unsigned)bar5
            );
            continue;
        }

        uintptr_t abar_phys = (uintptr_t)(bar5 & 0xfffffff0u);
        uintptr_t abar_virt = phys_to_virt(abar_phys);

        uint32_t cap = mmio_read32(abar_virt, AHCI_REG_CAP);
        uint32_t pi = mmio_read32(abar_virt, AHCI_REG_PI);

        printf(
            "sata%u controller ahci      %u:%u.%u       abar=0x%x pi=0x%x cap=0x%x\n",
            (unsigned)i,
            (unsigned)ctrl->bus,
            (unsigned)ctrl->slot,
            (unsigned)ctrl->func,
            (unsigned)abar_phys,
            (unsigned)pi,
            (unsigned)cap
        );

        for (int port = 0; port < 32; ++port) {
            if ((pi & (1u << port)) == 0) {
                continue;
            }

            uintptr_t port_base =
                abar_virt + AHCI_PORT_BASE + (uintptr_t)port * AHCI_PORT_SIZE;

            uint32_t sig = mmio_read32(port_base, AHCI_PORT_SIG);
            uint32_t ssts = mmio_read32(port_base, AHCI_PORT_SSTS);

            printf(
                "sd%c    %-8s  %-10s  %u:%u.%u       port=%u sig=0x%x ssts=0x%x\n",
                (char)('a' + port),
                ahci_device_type(sig),
                ahci_link_state(ssts),
                (unsigned)ctrl->bus,
                (unsigned)ctrl->slot,
                (unsigned)ctrl->func,
                (unsigned)port,
                (unsigned)sig,
                (unsigned)ssts
            );
        }
    }
}