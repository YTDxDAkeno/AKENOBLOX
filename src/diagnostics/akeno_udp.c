/* AkenoBlox PS5 - bounded UDP loopback probe, no remote traffic.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_udp.h"
#include "akeno_platform.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int
akeno_udp_loopback_probe(char *summary, size_t summary_size)
{
   if (summary == NULL || summary_size == 0)
      return -1;
   summary[0] = '\0';

   int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if (fd < 0) {
      snprintf(summary, summary_size, "UDP socket unavailable (errno %d)", errno);
      return -1;
   }

   struct sockaddr_in addr;
   memset(&addr, 0, sizeof(addr));
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   addr.sin_port = htons(0); /* kernel picks an unused local port */
   if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
      snprintf(summary, summary_size, "UDP loopback bind failed (errno %d)", errno);
      close(fd);
      return -1;
   }

   socklen_t addr_len = sizeof(addr);
   if (getsockname(fd, (struct sockaddr *)&addr, &addr_len) != 0 ||
       addr_len < sizeof(addr)) {
      snprintf(summary, summary_size, "UDP getsockname failed (errno %d)", errno);
      close(fd);
      return -1;
   }

   static const char packet[] = "akenoblox-udp-probe";
   if (sendto(fd, packet, sizeof(packet), 0, (const struct sockaddr *)&addr,
              sizeof(addr)) != (ssize_t)sizeof(packet)) {
      snprintf(summary, summary_size, "UDP sendto failed (errno %d)", errno);
      close(fd);
      return -1;
   }

   /* MSG_DONTWAIT ensures the probe never hangs on recvfrom, even if socket
    * timeouts are not implemented by the console's libc. Bound the loop and
    * pause via the portable platform layer. */
   for (unsigned attempt = 0; attempt < 250; attempt++) {
      char received[sizeof(packet)];
      const ssize_t got = recvfrom(fd, received, sizeof(received), MSG_DONTWAIT, NULL, NULL);
      if (got == (ssize_t)sizeof(packet) &&
          memcmp(received, packet, sizeof(packet)) == 0) {
         snprintf(summary, summary_size, "UDP IPv4 loopback: sent and received %zu bytes",
                  sizeof(packet));
         close(fd);
         return 0;
      }
      if (got >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
         snprintf(summary, summary_size, "UDP loopback recv failed (errno %d)",
                  got < 0 ? errno : 0);
         close(fd);
         return -1;
      }
      akeno_platform_sleep_us(1000);
   }

   snprintf(summary, summary_size, "UDP loopback timed out (250 attempts)");
   close(fd);
   return -1;
}
