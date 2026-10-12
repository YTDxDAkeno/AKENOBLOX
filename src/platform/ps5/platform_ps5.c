/*
 * AkenoBlox PS5 - the platform on the console.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Start-up order matters on the console (PS5_VulkanTemplate
 * platform-contracts.md): standard error into klog first, so everything after
 * leaves a record; the splash dismissed before the first present, or a title
 * presenting perfectly shows black; and the title ends by asking the shell to
 * close it, never by exit().
 */
#include "ps5_internal.h"

#include <ps5platform/kernel.h>
#include <ps5platform/klog.h>
#include <ps5platform/memory.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

static struct timespec start_time;
static char storage_root[64];

/* ---------------------------------------------------------- lifecycle */

int
akeno_platform_init(const char *title)
{
   clock_gettime(CLOCK_MONOTONIC, &start_time);
   static char prefix[48];
   snprintf(prefix, sizeof(prefix), "[%s] ", title);
   const int captured = ps5_klog_capture_stderr(prefix);
   fprintf(stderr, "platform: standard error to klog: %s\n", captured == 0 ? "yes" : "no");
   fprintf(stderr, "platform: splash dismissed: %d\n", sceSystemServiceHideSplashScreen());
   fprintf(stderr, "platform: user service: %d\n", sceUserServiceInitialize(NULL));
   if (!akeno_ps5_pad_open())
      fprintf(stderr, "platform: no controller yet\n");
   fflush(stderr);
   return 0;
}

void
akeno_platform_shutdown(void)
{
   akeno_platform_display_close();
   akeno_ps5_pad_close();
   fflush(NULL);
}

_Noreturn void
akeno_platform_exit(int status)
{
   fprintf(stderr, "platform: exit status %d: asking the shell to close the title\n", status);
   fflush(NULL);
   (void)sceSystemServiceLoadExec("exit", NULL);
   for (;;)
      sceKernelUsleep(100000); /* the shell closes the title asynchronously */
}

/* PS5_Vulkan's CRT calls this after main returns, in place of exit(). */
void catchReturnFromMain(int status);
void
catchReturnFromMain(int status)
{
   akeno_platform_exit(status);
}

/* -------------------------------------------------------------- facts */

void
akeno_platform_query(akeno_platform_info *info)
{
   memset(info, 0, sizeof(*info));
   snprintf(info->platform, sizeof(info->platform), "ps5");
   struct ps5_kernel_sw_version version;
   memset(&version, 0, sizeof(version));
   version.size = sizeof(version);
   if (sceKernelGetSystemSwVersion(&version) == 0 && version.text[0] != '\0')
      snprintf(info->firmware, sizeof(info->firmware), "%.27s (0x%08x)", version.text,
               version.version);
   else
      snprintf(info->firmware, sizeof(info->firmware), "unknown");
   struct ps5_memory_stats stats;
   if (ps5_memory_query(&stats)) {
      info->memory_known = true;
      info->memory_total = stats.total_bytes;
      info->memory_free = stats.free_bytes;
      info->memory_largest = stats.largest_bytes;
      info->flexible_free = stats.flexible_bytes;
   }
   info->page_size = PS5_KERNEL_PAGE_SIZE;
}

const akeno_mem_ops *
akeno_platform_code_memory(void)
{
   return akeno_ps5_code_memory();
}

/* --------------------------------------------------------------- time */

uint64_t
akeno_platform_now_us(void)
{
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   const int64_t micros =
      (int64_t)(now.tv_sec - start_time.tv_sec) * 1000000 + (now.tv_nsec - start_time.tv_nsec) / 1000;
   return micros > 0 ? (uint64_t)micros : 0;
}

void
akeno_platform_sleep_us(uint32_t microseconds)
{
   sceKernelUsleep(microseconds);
}

/* ------------------------------------------------------------ logging */

void
akeno_platform_log_line(const char *line)
{
   fputs(line, stderr);
   fputc('\n', stderr);
   fflush(stderr);
}

/* ------------------------------------------------------------ storage */

/* A directory is usable when a file can be created in it. A title sees /app0
 * and /download0 until it asks for /data; some loaders show /data to every
 * title (prospero-win docs/WINE_PS5_BUILD.md, "Data directory"), so it is
 * tried first. Folders 0777 and files 0666, so FTP can read them back
 * (PS5_VulkanTemplate platform-contracts.md). */
static bool
usable(const char *directory)
{
   if (mkdir(directory, 0777) != 0 && errno != EEXIST)
      return false;
   chmod(directory, 0777);
   char probe[96];
   snprintf(probe, sizeof(probe), "%s/.akeno-probe", directory);
   FILE *f = fopen(probe, "w");
   if (f == NULL)
      return false;
   const bool ok = fputs("akenoblox", f) >= 0;
   fclose(f);
   remove(probe);
   return ok;
}

const char *
akeno_platform_storage_root(void)
{
   if (storage_root[0] != '\0')
      return storage_root;
   static const char *const candidates[] = {"/data/akenoblox", "/download0/akenoblox"};
   for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
      if (usable(candidates[i])) {
         snprintf(storage_root, sizeof(storage_root), "%s", candidates[i]);
         return storage_root;
      }
   /* Nowhere writable: settings and logs stay in memory and klog. */
   snprintf(storage_root, sizeof(storage_root), "/app0");
   return storage_root;
}

const char *
akeno_platform_app_root(void)
{
   return "/app0";
}

/* ------------------------------------------------------------ network */

int
akeno_platform_network_check(const char *host, char *summary, size_t summary_size)
{
   /* getaddrinfo is the platform layer's ps5_getaddrinfo, bound by the link
    * recipe (PS5_Vulkan tools/radv-link.sh). */
   struct addrinfo hints, *result = NULL;
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   const int rc = getaddrinfo(host, "443", &hints, &result);
   if (rc != 0 || result == NULL) {
      snprintf(summary, summary_size, "%s does not resolve (%d)", host, rc);
      return -1;
   }
   int count = 0;
   for (struct addrinfo *a = result; a != NULL; a = a->ai_next)
      count++;
   freeaddrinfo(result);
   snprintf(summary, summary_size, "%s resolves (%d addresses)", host, count);
   return 0;
}
