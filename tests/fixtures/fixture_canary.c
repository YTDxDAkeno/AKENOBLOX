/*
 * AkenoBlox PS5 - loader fixture: Android's stack protector. On x86-64 bionic
 * keeps the canary at %fs:0x28 (TLS_SLOT_STACK_GUARD), and code built for
 * Android reads it there in every protected function's prologue and epilogue.
 * glibc happens to keep its own guard at the same offset; FreeBSD-derived
 * systems do not. Built with -fstack-protector-all, this measures whether
 * that word is stable across a call on the platform that loads it.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fixture_common.h"

FIXTURE_EXPORT int
canary_protected_sum(int n)
{
   volatile char buffer[64];
   int sum = 0;
   for (int i = 0; i < 64; i++)
      buffer[i] = (char)(i + n);
   for (int i = 0; i < 64; i++)
      sum += buffer[i];
   akeno_host_note("canary");
   return sum;
}
