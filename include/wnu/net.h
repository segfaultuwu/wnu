#ifndef WNU_NET_H
#define WNU_NET_H

#include <stddef.h>
#include <stdint.h>

void net_init(void);
void net_handle_packet(const uint8_t *packet, size_t len);

void net_ping_qemu_gateway(void);
void net_udp_test(void);

#endif