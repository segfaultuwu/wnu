#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "wnu/net.h"
#include "wnu/rtl8139.h"

#define ETH_TYPE_IPV4 0x0800
#define IP_PROTO_ICMP 1
#define IP_PROTO_UDP  17

static uint8_t local_ip[4] = {10, 0, 2, 15};
static uint8_t gateway_ip[4] = {10, 0, 2, 2};

/*
 * QEMU user-net gateway MAC.
 */
static uint8_t gateway_mac[6] = {
    0x52, 0x55, 0x0a, 0x00, 0x02, 0x02
};

static void memory_copy(uint8_t *dst, const uint8_t *src, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        dst[i] = src[i];
    }
}

static int ip_equal(const uint8_t *a, const uint8_t *b) {
    for (int i = 0; i < 4; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }

    return 1;
}

static uint16_t checksum(const void *data, size_t len) {
    const uint8_t *bytes = data;
    uint32_t sum = 0;

    for (size_t i = 0; i + 1 < len; i += 2) {
        sum += ((uint16_t)bytes[i] << 8) | bytes[i + 1];
    }

    if ((len & 1u) != 0) {
        sum += (uint16_t)bytes[len - 1] << 8;
    }

    while ((sum >> 16) != 0) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }

    return (uint16_t)~sum;
}

void net_init(void) {
}

static void send_icmp_echo(void) {
    uint8_t packet[14 + 20 + 8 + 32];

    const uint8_t *local_mac = rtl8139_get_mac();

    memory_copy(packet + 0, gateway_mac, 6);
    memory_copy(packet + 6, local_mac, 6);

    packet[12] = 0x08;
    packet[13] = 0x00;

    uint8_t *ip = packet + 14;

    ip[0] = 0x45;
    ip[1] = 0x00;

    uint16_t total_len = 20 + 8 + 32;

    ip[2] = (uint8_t)(total_len >> 8);
    ip[3] = (uint8_t)(total_len & 0xff);

    ip[4] = 0x12;
    ip[5] = 0x34;

    ip[6] = 0x00;
    ip[7] = 0x00;

    ip[8] = 64;
    ip[9] = IP_PROTO_ICMP;

    ip[10] = 0;
    ip[11] = 0;

    memory_copy(ip + 12, local_ip, 4);
    memory_copy(ip + 16, gateway_ip, 4);

    uint16_t ip_sum = checksum(ip, 20);
    ip[10] = (uint8_t)(ip_sum >> 8);
    ip[11] = (uint8_t)(ip_sum & 0xff);

    uint8_t *icmp = ip + 20;

    icmp[0] = 8;
    icmp[1] = 0;
    icmp[2] = 0;
    icmp[3] = 0;

    icmp[4] = 0x00;
    icmp[5] = 0x01;

    icmp[6] = 0x00;
    icmp[7] = 0x01;

    for (int i = 0; i < 32; ++i) {
        icmp[8 + i] = (uint8_t)i;
    }

    uint16_t icmp_sum = checksum(icmp, 8 + 32);
    icmp[2] = (uint8_t)(icmp_sum >> 8);
    icmp[3] = (uint8_t)(icmp_sum & 0xff);

    rtl8139_send(packet, sizeof(packet));

    printf("ping: icmp echo sent\n");
}

void net_ping_qemu_gateway(void) {
    send_icmp_echo();
}

void net_udp_test(void) {
    static const char message[] = "hello from WNU\n";

    uint16_t src_port = 4444;
    uint16_t dst_port = 5555;

    size_t payload_len = sizeof(message) - 1;
    size_t udp_len = 8 + payload_len;
    size_t ip_len = 20 + udp_len;
    size_t packet_len = 14 + ip_len;

    uint8_t packet[14 + 20 + 8 + 64];

    const uint8_t *local_mac = rtl8139_get_mac();

    memory_copy(packet + 0, gateway_mac, 6);
    memory_copy(packet + 6, local_mac, 6);

    packet[12] = 0x08;
    packet[13] = 0x00;

    uint8_t *ip = packet + 14;

    ip[0] = 0x45;
    ip[1] = 0x00;

    ip[2] = (uint8_t)(ip_len >> 8);
    ip[3] = (uint8_t)(ip_len & 0xff);

    ip[4] = 0xab;
    ip[5] = 0xcd;

    ip[6] = 0x00;
    ip[7] = 0x00;

    ip[8] = 64;
    ip[9] = IP_PROTO_UDP;

    ip[10] = 0;
    ip[11] = 0;

    memory_copy(ip + 12, local_ip, 4);
    memory_copy(ip + 16, gateway_ip, 4);

    uint16_t ip_sum = checksum(ip, 20);
    ip[10] = (uint8_t)(ip_sum >> 8);
    ip[11] = (uint8_t)(ip_sum & 0xff);

    uint8_t *udp = ip + 20;

    udp[0] = (uint8_t)(src_port >> 8);
    udp[1] = (uint8_t)(src_port & 0xff);

    udp[2] = (uint8_t)(dst_port >> 8);
    udp[3] = (uint8_t)(dst_port & 0xff);

    udp[4] = (uint8_t)(udp_len >> 8);
    udp[5] = (uint8_t)(udp_len & 0xff);

    /*
     * UDP checksum 0 means disabled for IPv4.
     */
    udp[6] = 0;
    udp[7] = 0;

    memory_copy(udp + 8, (const uint8_t *)message, payload_len);

    rtl8139_send(packet, packet_len);

    printf("udp: sent to 10.0.2.2:5555\n");
}

void net_handle_packet(const uint8_t *packet, size_t len) {
    if (len < 14) {
        return;
    }

    uint16_t eth_type = ((uint16_t)packet[12] << 8) | packet[13];

    if (eth_type != ETH_TYPE_IPV4) {
        return;
    }

    if (len < 14 + 20) {
        return;
    }

    const uint8_t *ip = packet + 14;

    if (!ip_equal(ip + 16, local_ip)) {
        return;
    }

    uint8_t ihl = (uint8_t)((ip[0] & 0x0f) * 4);

    if (len < 14 + ihl) {
        return;
    }

    if (ip[9] == IP_PROTO_ICMP) {
        const uint8_t *icmp = ip + ihl;

        if (len < 14 + ihl + 8) {
            return;
        }

        if (icmp[0] == 0) {
            printf("ping: reply from 10.0.2.2\n");
        }

        return;
    }

    if (ip[9] == IP_PROTO_UDP) {
        printf("net: udp packet received\n");
        return;
    }
}