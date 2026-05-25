#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "wnu/net.h"
#include "wnu/rtl8139.h"
#include "wnu/platform.h"

#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IPV4 0x0800

#define IP_PROTO_ICMP 1
#define IP_PROTO_UDP  17

#define ARP_OPCODE_REQUEST 1
#define ARP_OPCODE_REPLY   2

static uint8_t local_ip[4] = {10, 0, 2, 15};
static uint8_t gateway_ip[4] = {10, 0, 2, 2};

static uint8_t gateway_mac[6];
static int gateway_mac_ready;
static int pending_icmp;
static int pending_udp;

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
    gateway_mac_ready = 0;
    pending_icmp = 0;
    pending_udp = 0;
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
    wnu_serial_write("ping: icmp echo sent\n");
}

static void send_udp_packet(void) {
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

    /* UDP checksum 0 means disabled for IPv4. */
    udp[6] = 0;
    udp[7] = 0;

    memory_copy(udp + 8, (const uint8_t *)message, payload_len);

    rtl8139_send(packet, packet_len);

    printf("udp: sent to 10.0.2.2:5555\n");
    wnu_serial_write("udp: sent to 10.0.2.2:5555\n");
}

static void send_arp_request(void);

static void request_gateway_mac(int want_icmp, int want_udp) {
    pending_icmp |= want_icmp;
    pending_udp |= want_udp;

    if (gateway_mac_ready) {
        if (pending_icmp) {
            pending_icmp = 0;
            send_icmp_echo();
        }

        if (pending_udp) {
            pending_udp = 0;
            send_udp_packet();
        }

        return;
    }

    printf("net: resolving gateway mac...\n");
    wnu_serial_write("net: resolving gateway mac...\n");
    send_arp_request();
}

static void send_arp_request(void) {
    uint8_t packet[42];

    const uint8_t broadcast[6] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    const uint8_t zero_mac[6] = {0, 0, 0, 0, 0, 0};
    const uint8_t *local_mac = rtl8139_get_mac();

    memory_copy(packet + 0, broadcast, 6);
    memory_copy(packet + 6, local_mac, 6);

    packet[12] = 0x08;
    packet[13] = 0x06;

    packet[14] = 0x00;
    packet[15] = 0x01;

    packet[16] = 0x08;
    packet[17] = 0x00;

    packet[18] = 6;
    packet[19] = 4;

    packet[20] = 0x00;
    packet[21] = ARP_OPCODE_REQUEST;

    memory_copy(packet + 22, local_mac, 6);
    memory_copy(packet + 28, local_ip, 4);
    memory_copy(packet + 32, zero_mac, 6);
    memory_copy(packet + 38, gateway_ip, 4);

    rtl8139_send(packet, sizeof(packet));
}

void net_ping_qemu_gateway(void) {
    request_gateway_mac(1, 0);
}

void net_udp_test(void) {
    request_gateway_mac(0, 1);
}

void net_handle_packet(const uint8_t *packet, size_t len) {
    if (len < 14) {
        return;
    }

    uint16_t eth_type = ((uint16_t)packet[12] << 8) | packet[13];

    if (eth_type == ETH_TYPE_ARP && len >= 42) {
        const uint8_t *arp = packet + 14;

        uint16_t opcode = ((uint16_t)arp[6] << 8) | arp[7];

        if (opcode != ARP_OPCODE_REPLY) {
            return;
        }

        const uint8_t *sender_mac = arp + 8;
        const uint8_t *sender_ip = arp + 14;
        const uint8_t *target_ip = arp + 24;

        if (ip_equal(sender_ip, gateway_ip) && ip_equal(target_ip, local_ip)) {
            memory_copy(gateway_mac, sender_mac, 6);
            gateway_mac_ready = 1;

            printf(
                "arp: gateway mac %x:%x:%x:%x:%x:%x\n",
                gateway_mac[0],
                gateway_mac[1],
                gateway_mac[2],
                gateway_mac[3],
                gateway_mac[4],
                gateway_mac[5]
            );

            if (pending_icmp) {
                pending_icmp = 0;
                send_icmp_echo();
            }

            if (pending_udp) {
                pending_udp = 0;
                send_udp_packet();
            }
        }

        return;
    }

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
