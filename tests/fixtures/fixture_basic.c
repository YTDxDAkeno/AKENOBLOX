/*
 * AkenoBlox PS5 - loader fixture: every relocation shape a C library makes.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Relative pointers in data and in RELRO, a call through the PLT, the address
 * of an import (GLOB_DAT), a pointer to an exported function (a symbolic
 * R_X86_64_64), a constructor and a destructor. Linked several ways
 * (tests/CMakeLists.txt): plain RELA, APS2, RELR, SysV and GNU hashes, 4 KiB
 * and 16 KiB pages.
 */
#include "fixture_common.h"

static int counter;
static int destroyed_marker;

FIXTURE_EXPORT int fixture_data[4] = {1, 2, 3, 4};
FIXTURE_EXPORT const char *fixture_greeting = "akeno";

static int twice(int x) { return x * 2; }
static int thrice(int x) { return x * 3; }
/* const: lands in .data.rel.ro, inside PT_GNU_RELRO */
static int (*const operations[2])(int) = {twice, thrice};

FIXTURE_EXPORT int fixture_counter(void);
/* a preemptible definition referenced by address: R_X86_64_64 with a symbol */
FIXTURE_EXPORT int (*fixture_counter_pointer)(void) = fixture_counter;

__attribute__((constructor)) static void
fixture_init(void)
{
   counter = 42;
   akeno_host_note("init");
}

__attribute__((destructor)) static void
fixture_fini(void)
{
   destroyed_marker = 1;
   akeno_host_note("fini");
}

FIXTURE_EXPORT int fixture_counter(void) { return counter; }
FIXTURE_EXPORT int fixture_add(int a, int b) { return akeno_host_add(a, b); }
FIXTURE_EXPORT int fixture_apply(int which, int x) { return operations[which & 1](x); }
FIXTURE_EXPORT const char *fixture_greet(void) { return fixture_greeting; }
FIXTURE_EXPORT int *fixture_data_pointer(void) { return fixture_data; }
FIXTURE_EXPORT void *fixture_import_address(void) { return (void *)&akeno_host_add; }
FIXTURE_EXPORT int fixture_destroyed(void) { return destroyed_marker; }

FIXTURE_EXPORT int
fixture_sum_data(void)
{
   volatile int *data = fixture_data;
   return data[0] + data[1] + data[2] + data[3];
}

FIXTURE_EXPORT void
fixture_write_data(int index, int value)
{
   fixture_data[index & 3] = value;
}
