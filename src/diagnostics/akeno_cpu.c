/*
 * AkenoBlox PS5 - CPU facts.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_cpu.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#if defined(__x86_64__)
static void
cpuid(uint32_t leaf, uint32_t subleaf, uint32_t r[4])
{
   __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(leaf), "c"(subleaf));
}

static uint64_t
xgetbv0(void)
{
   uint32_t lo, hi;
   __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
   return ((uint64_t)hi << 32) | lo;
}
#endif

int
akeno_cpu_level(uint32_t f, bool ymm_enabled)
{
   const uint32_t v2 = AKENO_CPU_CX16 | AKENO_CPU_LAHF | AKENO_CPU_POPCNT | AKENO_CPU_SSE3 |
                       AKENO_CPU_SSSE3 | AKENO_CPU_SSE41 | AKENO_CPU_SSE42;
   const uint32_t v3 = AKENO_CPU_AVX | AKENO_CPU_AVX2 | AKENO_CPU_BMI1 | AKENO_CPU_BMI2 |
                       AKENO_CPU_F16C | AKENO_CPU_FMA | AKENO_CPU_LZCNT | AKENO_CPU_MOVBE |
                       AKENO_CPU_OSXSAVE;
   if ((f & v2) != v2)
      return 1;
   if ((f & v3) != v3 || !ymm_enabled)
      return 2;
   if (!(f & AKENO_CPU_AVX512F))
      return 3;
   return 4;
}

void
akeno_cpu_query(akeno_cpu_info *info)
{
   memset(info, 0, sizeof(*info));
#if defined(__x86_64__)
   uint32_t r[4];
   cpuid(0, 0, r);
   const uint32_t max_leaf = r[0];
   memcpy(info->vendor + 0, &r[1], 4);
   memcpy(info->vendor + 4, &r[3], 4);
   memcpy(info->vendor + 8, &r[2], 4);
   info->vendor[12] = '\0';
   info->available = true;

   uint32_t f = 0;
   if (max_leaf >= 1) {
      cpuid(1, 0, r);
      const uint32_t base_family = (r[0] >> 8) & 0xf, base_model = (r[0] >> 4) & 0xf;
      info->stepping = r[0] & 0xf;
      info->family = base_family == 0xf ? base_family + ((r[0] >> 20) & 0xff) : base_family;
      info->model = (base_family == 0x6 || base_family == 0xf)
                       ? (((r[0] >> 16) & 0xf) << 4) | base_model
                       : base_model;
      info->logical_per_package = (r[1] >> 16) & 0xff;
      if (r[2] & (1u << 0))
         f |= AKENO_CPU_SSE3;
      if (r[2] & (1u << 9))
         f |= AKENO_CPU_SSSE3;
      if (r[2] & (1u << 12))
         f |= AKENO_CPU_FMA;
      if (r[2] & (1u << 13))
         f |= AKENO_CPU_CX16;
      if (r[2] & (1u << 19))
         f |= AKENO_CPU_SSE41;
      if (r[2] & (1u << 20))
         f |= AKENO_CPU_SSE42;
      if (r[2] & (1u << 22))
         f |= AKENO_CPU_MOVBE;
      if (r[2] & (1u << 23))
         f |= AKENO_CPU_POPCNT;
      if (r[2] & (1u << 27))
         f |= AKENO_CPU_OSXSAVE;
      if (r[2] & (1u << 28))
         f |= AKENO_CPU_AVX;
      if (r[2] & (1u << 29))
         f |= AKENO_CPU_F16C;
   }
   if (max_leaf >= 7) {
      cpuid(7, 0, r);
      if (r[1] & (1u << 3))
         f |= AKENO_CPU_BMI1;
      if (r[1] & (1u << 5))
         f |= AKENO_CPU_AVX2;
      if (r[1] & (1u << 8))
         f |= AKENO_CPU_BMI2;
      if (r[1] & (1u << 16))
         f |= AKENO_CPU_AVX512F;
   }
   cpuid(0x80000000u, 0, r);
   const uint32_t max_extended = r[0];
   if (max_extended >= 0x80000001u) {
      cpuid(0x80000001u, 0, r);
      if (r[2] & (1u << 0))
         f |= AKENO_CPU_LAHF;
      if (r[2] & (1u << 5))
         f |= AKENO_CPU_LZCNT;
   }
   if (max_extended >= 0x80000004u) {
      for (uint32_t leaf = 0; leaf < 3; leaf++) {
         cpuid(0x80000002u + leaf, 0, r);
         memcpy(info->brand + leaf * 16, r, 16);
      }
      info->brand[48] = '\0';
      /* Brand strings are padded with leading spaces on some parts. */
      size_t lead = 0;
      while (info->brand[lead] == ' ')
         lead++;
      memmove(info->brand, info->brand + lead, sizeof(info->brand) - lead);
   }
   info->features = f;
   if (f & AKENO_CPU_OSXSAVE)
      info->ymm_enabled = (xgetbv0() & 0x6u) == 0x6u;
   info->level = akeno_cpu_level(f, info->ymm_enabled);
#endif
}

bool
akeno_cpu_meets_android_floor(const akeno_cpu_info *info)
{
   return info != NULL && info->available && info->level >= 2;
}

/* ---------------------------------------------------------- %fs:0x28 */

static uint64_t
read_fs_0x28(void)
{
#if defined(__x86_64__)
   uint64_t value;
   __asm__ volatile("movq %%fs:0x28, %0" : "=r"(value));
   return value;
#else
   return 0;
#endif
}

/* Allocation and stack traffic between the two reads: if %fs:0x28 were heap
 * memory just past the thread block, this is what would change it. */
static void
churn(void)
{
   volatile uint8_t scratch[4096];
   for (unsigned i = 0; i < sizeof(scratch); i++)
      scratch[i] = (uint8_t)i;
   for (size_t i = 0; i < 64; i++) {
      uint8_t *block = malloc(64 + i * 32);
      if (block != NULL) {
         memset(block, 0xa5, 64 + i * 32);
         free(block);
      }
   }
}

static void *
thread_probe(void *arg)
{
   akeno_fs_probe *probe = arg;
   probe->thread_first = read_fs_0x28();
   churn();
   probe->thread_second = read_fs_0x28();
   probe->thread_stable = probe->thread_first == probe->thread_second;
   probe->thread_ran = true;
   return NULL;
}

void
akeno_fs_probe_run(akeno_fs_probe *probe)
{
   memset(probe, 0, sizeof(*probe));
   probe->main_first = read_fs_0x28();
   churn();
   probe->main_second = read_fs_0x28();
   probe->main_stable = probe->main_first == probe->main_second;
   probe->ran = true;
   pthread_t thread;
   if (pthread_create(&thread, NULL, thread_probe, probe) == 0)
      pthread_join(thread, NULL);
   probe->differs_between_threads = probe->thread_ran && probe->thread_first != probe->main_first;
}
