/* AkenoBlox PS5 - bounded local UDP datagram capability probe.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Proves only that a UDP socket can send to itself on 127.0.0.1.
 * Does NOT prove outbound UDP to Roblox multiplayer servers.
 */
#ifndef AKENO_UDP_H
#define AKENO_UDP_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Fills summary on both success and failure. Returns 0 for a successful
 * local datagram round-trip; -1 for unavailable/error/timeout. */
int akeno_udp_loopback_probe(char *summary, size_t summary_size);
#ifdef __cplusplus
}
#endif
#endif
