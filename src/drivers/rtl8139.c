#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "wnu/net.h"
#include "wnu/pci.h"
#include "wnu/platform.h"
#include "wnu/rtl8139.h"

#define RTL8139_VENDOR 0x10ec
#define RTL8139_DEVICE 0x8139

#define REG_MAC0       0x00
#define REG_TX_STATUS0 0x10
#define REG_TX_ADDR0   0x20
#define REG_RX_BUF     0x30
#define REG_CMD        0x37
#define REG_CAPR       0x38
#define REG_CBR        0x3A
#define REG_IMR        0x3C
#define REG_ISR        0x3E
#define REG_TX_CONFIG  0x40
#define REG_RX_CONFIG  0x44
#define REG_CONFIG1    0x52

#define CMD_RX_EMPTY 0x01
#define CMD_TX_EN    0x04
#define CMD_RX_EN    0x08
#define CMD_RESET    0x10

#define ISR_RX_OK    0x0001
#define ISR_RX_ERR   0x0002
#define ISR_TX_OK    0x0004
#define ISR_TX_ERR   0x0008

#define RX_BUFFER_SIZE (8192 + 16 + 1500)
#define TX_BUFFER_SIZE 2048

static uint16_t io_base;
static uint8_t rx_buffer[RX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[4][TX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t mac[6];

static uint32_t tx_index;
static uint16_t rx_offset;

static uint8_t rtl_read8(uint16_t reg) {
    return wnu_inb(io_base + reg);
}

static uint16_t rtl_read16(uint16_t reg) {
    uint16_t v = 0;
    v |= (uint16_t)wnu_inb(io_base + reg);
    v |= (uint16_t)wnu_inb(io_base + reg + 1) << 8;
    return v;
}

static uint32_t rtl_read32(uint16_t reg) {
    return wnu_inl(io_base + reg);
}

static void rtl_write8(uint16_t reg, uint8_t value) {
    wnu_outb(io_base + reg, value);
}

static void rtl_write16(uint16_t reg, uint16_t value) {
    wnu_outb(io_base + reg, (uint8_t)(value & 0xff));
    wnu_outb(io_base + reg + 1, (uint8_t)((value >> 8) & 0xff));
}

static void rtl_write32(uint16_t reg, uint32_t value) {
    wnu_outl(io_base + reg, value);
}

static void memory_copy(uint8_t *dst, const uint8_t *src, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        dst[i] = src[i];
    }
}

const uint8_t *rtl8139_get_mac(void) {
    return mac;
}

void rtl8139_send(const uint8_t *data, size_t len) {
    if (io_base == 0 || data == 0) {
        return;
    }

    if (len > TX_BUFFER_SIZE) {
        len = TX_BUFFER_SIZE;
    }

    size_t tx_len = len;

    if (tx_len < 60) {
        tx_len = 60;
    }

    uint32_t slot = tx_index;
    uint8_t *buf = tx_buffers[slot];

    for (size_t i = 0; i < tx_len; ++i) {
        buf[i] = 0;
    }

    memory_copy(buf, data, len);

    uintptr_t phys = wnu_virt_to_phys(buf);

    if (phys > 0xffffffffu) {
        printf("rtl8139: tx buffer above 4GiB\n");
        return;
    }

    rtl_write32(REG_TX_ADDR0 + slot * 4, (uint32_t)phys);
    rtl_write32(REG_TX_STATUS0 + slot * 4, (uint32_t)tx_len);

    for (volatile uint32_t i = 0; i < 1000000; ++i) {
    }

    uint32_t status = rtl_read32(REG_TX_STATUS0 + slot * 4);

    printf(
        "rtl8139: send len=%u tx=%u phys=0x%x status=0x%x\n",
        (unsigned)tx_len,
        (unsigned)slot,
        (uint32_t)phys,
        status
    );

    tx_index = (tx_index + 1) & 3;
}

void rtl8139_poll(void) {
    if (io_base == 0) {
        return;
    }

    uint8_t cmd = rtl_read8(REG_CMD);
    uint16_t isr = rtl_read16(REG_ISR);

    if ((cmd & CMD_RX_EMPTY) != 0 && isr == 0) {
        return;
    }

    printf(
        "rtl8139: poll cmd=0x%x isr=0x%x cbr=0x%x capr=0x%x\n",
        cmd,
        isr,
        rtl_read16(REG_CBR),
        rtl_read16(REG_CAPR)
    );

    while ((rtl_read8(REG_CMD) & CMD_RX_EMPTY) == 0) {
        uint8_t *packet = rx_buffer + rx_offset;

        uint16_t status =
            (uint16_t)packet[0] |
            ((uint16_t)packet[1] << 8);

        uint16_t len =
            (uint16_t)packet[2] |
            ((uint16_t)packet[3] << 8);

        printf(
            "rtl8139: rx status=0x%x len=%u off=%u\n",
            status,
            len,
            rx_offset
        );

        if ((status & 0x0001) == 0) {
            printf("rtl8139: bad rx status\n");
            rtl_write16(REG_ISR, ISR_RX_ERR);
            break;
        }

        if (len < 4 || len > 1600) {
            printf("rtl8139: bad rx len\n");
            rtl_write16(REG_ISR, ISR_RX_ERR);
            break;
        }

        /*
         * RTL8139 includes the Ethernet FCS in the reported length.
         * Pass the packet without the 4-byte status/length header and
         * without the trailing FCS.
         */
        net_handle_packet(packet + 4, len - 4);

        rx_offset = (uint16_t)((rx_offset + len + 4 + 3) & ~3u);

        if (rx_offset >= 8192) {
            rx_offset -= 8192;
        }

        /*
         * CAPR is documented as the current RX offset minus 16.
         * At offset 0, writing 0 is safer than underflowing.
         */
        uint16_t capr = rx_offset;

        if (capr >= 16) {
            capr -= 16;
        } else {
            capr = 0;
        }

        rtl_write16(REG_CAPR, capr);
        rtl_write16(REG_ISR, ISR_RX_OK | ISR_RX_ERR);
    }

    if (isr & (ISR_TX_OK | ISR_TX_ERR)) {
        rtl_write16(REG_ISR, ISR_TX_OK | ISR_TX_ERR);
    }
}

void rtl8139_init(void) {
    struct pci_device dev;

    printf("rtl8139: probing...\n");

    if (!pci_find_device(RTL8139_VENDOR, RTL8139_DEVICE, &dev)) {
        printf("rtl8139: not found\n");
        io_base = 0;
        return;
    }

    uint32_t bar0 = pci_config_read32(dev.bus, dev.slot, dev.func, 0x10);

    if ((bar0 & 1u) == 0) {
        printf("rtl8139: BAR0 is not I/O space\n");
        io_base = 0;
        return;
    }

    io_base = (uint16_t)(bar0 & ~3u);

    printf("rtl8139: found io=0x%x\n", io_base);

    /*
     * Power on the chip.
     */
    rtl_write8(REG_CONFIG1, 0x00);

    /*
     * Reset the chip.
     */
    rtl_write8(REG_CMD, CMD_RESET);

    uint32_t timeout = 1000000;

    while ((rtl_read8(REG_CMD) & CMD_RESET) != 0 && timeout > 0) {
        timeout--;
    }

    if (timeout == 0) {
        printf("rtl8139: reset timeout\n");
        io_base = 0;
        return;
    }

    /*
     * Read the hardware MAC address.
     */
    for (int i = 0; i < 6; ++i) {
        mac[i] = rtl_read8(REG_MAC0 + i);
    }

    printf(
        "rtl8139: mac %x:%x:%x:%x:%x:%x\n",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]
    );

    tx_index = 0;
    rx_offset = 0;

    uintptr_t rx_phys = wnu_virt_to_phys(rx_buffer);

    printf(
        "rtl8139: rx virt=%p phys=0x%x\n",
        rx_buffer,
        (uint32_t)rx_phys
    );

    if (rx_phys > 0xffffffffu) {
        printf("rtl8139: rx buffer above 4GiB\n");
        io_base = 0;
        return;
    }

    /*
     * The RX buffer register expects a physical DMA address.
     */
    rtl_write32(REG_RX_BUF, (uint32_t)rx_phys);

    /*
     * Start at the beginning of the RX ring.
     */
    rtl_write16(REG_CAPR, 0);

    /*
     * Clear any pending status before enabling the chip.
     */
    rtl_write16(REG_ISR, 0xffff);

    /*
     * Enable RX and TX.
     */
    rtl_write8(REG_CMD, CMD_RX_EN | CMD_TX_EN);

    /*
     * RX configuration:
     * - MXDMA unlimited
     * - 8 KiB RX buffer
     * - WRAP enabled
     * - accept broadcast packets
     * - accept packets for our MAC
     * - accept all packets for early debugging
     */
    rtl_write32(
        REG_RX_CONFIG,
        (0u << 11) |
        (0u << 13) |
        (1u << 7)  |
        (1u << 3)  |
        (1u << 1)  |
        (1u << 0)
    );

    /*
     * TX configuration.
     */
    rtl_write32(REG_TX_CONFIG, 0x03000000);

    /*
     * Polling mode for now.
     */
    rtl_write16(REG_IMR, 0x0000);

    printf("rtl8139: initialized\n");
}
