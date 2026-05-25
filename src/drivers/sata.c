#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "wnu/console.h"
#include "wnu/pci.h"
#include "wnu/platform.h"
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

void wnu_sata_init(void) {
    printf("sata: init\n");

    sata_scan_pci();

    if (sata_ctrl_count == 0) {
        printf("sata: no SATA controllers found\n");
        return;
    }

    for (size_t i = 0; i < sata_ctrl_count; ++i) {
        sata_print_controller(&sata_ctrls[i], i);
        sata_probe_ahci_controller(&sata_ctrls[i], i);
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