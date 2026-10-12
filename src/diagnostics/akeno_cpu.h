/*
 * AkenoBlox PS5 - CPU facts the Android engine depends on.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Roblox's Android x86-64 engine is compiled for an x86-64-v2 baseline: Sober
 * requires SSE4.1/SSE4.2, and libbadcpu exists to trap the v2/BMI
 * instructions older CPUs lack (Cordial docs/findings.md, sections 1.1(a) and
 * 1.1(d)). The console's CPU is a Zen 2, which should clear v3; this reads it
 * from CPUID rather than assuming it.
 */
#ifndef AKENO_CPU_H
#define AKENO_CPU_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
   AKENO_CPU_SSE3 = 1u << 0,
   AKENO_CPU_SSSE3 = 1u << 1,
   AKENO_CPU_SSE41 = 1u << 2,
   AKENO_CPU_SSE42 = 1u << 3,
   AKENO_CPU_POPCNT = 1u << 4,
   AKENO_CPU_CX16 = 1u << 5,
   AKENO_CPU_LAHF = 1u << 6,
   AKENO_CPU_AVX = 1u << 7,
   AKENO_CPU_AVX2 = 1u << 8,
   AKENO_CPU_BMI1 = 1u << 9,
   AKENO_CPU_BMI2 = 1u << 10,
   AKENO_CPU_FMA = 1u << 11,
   AKENO_CPU_F16C = 1u << 12,
   AKENO_CPU_MOVBE = 1u << 13,
   AKENO_CPU_LZCNT = 1u << 14,
   AKENO_CPU_OSXSAVE = 1u << 15,
   AKENO_CPU_AVX512F = 1u << 16,
};

typedef struct akeno_cpu_info {
   bool available; /* CPUID ran */
   char vendor[13];
   char brand[49];
   uint32_t family, model, stepping;
   uint32_t features; /* AKENO_CPU_* */
   bool ymm_enabled;  /* the OS saves AVX state (XCR0 bits 1 and 2) */
   unsigned logical_per_package;
   int level;         /* x86-64 microarchitecture level: 1..4 */
} akeno_cpu_info;

/* Executes CPUID (and XGETBV when OSXSAVE is set). Not called at start-up:
 * the Diagnostics screen runs it on request, logging before it does, so a
 * console that refused the instruction would leave a record naming it. */
void akeno_cpu_query(akeno_cpu_info *info);

/* The level from a feature set, separated from the instruction for tests:
 * v2 = CX16 LAHF POPCNT SSE3 SSSE3 SSE4.1 SSE4.2; v3 adds AVX AVX2 BMI1 BMI2
 * F16C FMA LZCNT MOVBE with the OS saving YMM; v4 adds AVX-512F. */
int akeno_cpu_level(uint32_t features, bool ymm_enabled);

/* Whether Roblox's x86-64 build can be expected to run without instruction
 * emulation: the v2 floor. */
bool akeno_cpu_meets_android_floor(const akeno_cpu_info *info);

/* -------------------------------------------------- the %fs:0x28 probe */

/* Android's x86-64 stack protector reads its canary at %fs:0x28 (bionic's
 * TLS_SLOT_STACK_GUARD); the engine's protected functions compare the word
 * at entry and at return. Where %fs points at a FreeBSD-shaped thread block
 * that offset is not a canary, so what matters is whether the word is
 * readable and stable for a thread's life. This reads it on the calling
 * thread twice around some work, and on a fresh thread. */
typedef struct akeno_fs_probe {
   bool ran;
   uint64_t main_first, main_second;
   bool main_stable;
   bool thread_ran;
   uint64_t thread_first, thread_second;
   bool thread_stable;
   bool differs_between_threads;
} akeno_fs_probe;

void akeno_fs_probe_run(akeno_fs_probe *probe);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_CPU_H */
