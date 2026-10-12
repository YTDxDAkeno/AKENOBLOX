/*
 * AkenoBlox PS5 - the ELF loader's self-test: Phase 2's gate, run where the
 * program runs.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Loads the test libraries built into the program (the same bytes the host
 * tests load: tests/fixtures, embedded by cmake/AkenoFixtures.cmake) through
 * the platform's own code memory, calls into them and checks every answer.
 * On the console that is the first time foreign x86-64 code is copied into
 * direct memory, relocated, given execute by sceKernelMprotect and called.
 * Until a console has run it and passed, Phase 2 is not done
 * (docs/HARDWARE_TESTING.md).
 */
#ifndef AKENO_SELFTEST_H
#define AKENO_SELFTEST_H

#include <stdbool.h>
#include <stdint.h>

#include "akeno_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { AKENO_SELFTEST_MAX_STEPS = 20 };

typedef struct akeno_selftest_step {
   char name[48];
   bool passed;
   char detail[112];
} akeno_selftest_step;

typedef struct akeno_selftest_result {
   bool ran;
   bool passed;   /* every step passed */
   int step_count;
   int failed_count;
   akeno_selftest_step steps[AKENO_SELFTEST_MAX_STEPS];
   uint64_t duration_us;
   char fixture_sha256[65]; /* of the APS2 fixture, to tie a log to the bytes */
} akeno_selftest_result;

/* Runs every step against mem; logs each step before it runs, so a crash in
 * one is attributable from the log alone. include_canary also calls the
 * stack-protected fixture, which reads %fs:0x28 the way Android code does. */
void akeno_selftest_run(const akeno_mem_ops *mem, bool include_canary, akeno_selftest_result *out);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_SELFTEST_H */
