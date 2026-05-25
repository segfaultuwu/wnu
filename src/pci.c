#include <stdint.h>

#include "wnu/pci.h"
#include "wnu/platform.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

uint32_t pci_config_read32(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
) {
    uint32_t address =
        (1u << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        ((uint32_t)offset & 0xFC);

    wnu_outl(PCI_CONFIG_ADDRESS, address);
    return wnu_inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
) {
    uint32_t value = pci_config_read32(bus, slot, func, offset);
    return (uint16_t)((value >> ((offset & 2u) * 8u)) & 0xffffu);
}

uint8_t pci_config_read8(
    uint8_t bus,
    uint8_t slot,
    uint8_t func,
    uint8_t offset
) {
    uint32_t value = pci_config_read32(bus, slot, func, offset);
    return (uint8_t)((value >> ((offset & 3u) * 8u)) & 0xffu);
}

int pci_find_device(uint16_t vendor, uint16_t device, struct pci_device *out) {
    if (out == 0) {
        return 0;
    }

    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            uint16_t vendor_id = pci_config_read16((uint8_t)bus, slot, 0, 0x00);

            if (vendor_id == 0xffff) {
                continue;
            }

            uint8_t header_type = pci_config_read8((uint8_t)bus, slot, 0, 0x0E);
            uint8_t function_count = (header_type & 0x80) ? 8 : 1;

            for (uint8_t func = 0; func < function_count; ++func) {
                vendor_id = pci_config_read16((uint8_t)bus, slot, func, 0x00);

                if (vendor_id == 0xffff) {
                    continue;
                }

                uint16_t device_id =
                    pci_config_read16((uint8_t)bus, slot, func, 0x02);

                if (vendor_id == vendor && device_id == device) {
                    out->bus = (uint8_t)bus;
                    out->slot = slot;
                    out->func = func;
                    out->vendor_id = vendor_id;
                    out->device_id = device_id;
                    out->class_code =
                        pci_config_read8((uint8_t)bus, slot, func, 0x0B);
                    out->subclass =
                        pci_config_read8((uint8_t)bus, slot, func, 0x0A);
                    out->prog_if =
                        pci_config_read8((uint8_t)bus, slot, func, 0x09);

                    return 1;
                }
            }
        }
    }

    return 0;
}
