#ifndef WNU_PCI_H
#define WNU_PCI_H

#include <stdint.h>

struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
};

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

int pci_find_device(uint16_t vendor, uint16_t device, struct pci_device *out);

#endif
