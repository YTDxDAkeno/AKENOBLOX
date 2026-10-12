/*
 * AkenoBlox PS5 - loader fixture: IFUNC, exported (a symbol) and hidden
 * (R_X86_64_IRELATIVE), whose resolvers run during loading.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fixture_common.h"

static int implementation(void) { return 11; }
static void *resolve_picked(void) { return (void *)implementation; }

FIXTURE_EXPORT int picked(void) __attribute__((ifunc("resolve_picked")));
__attribute__((visibility("hidden"))) int hidden_picked(void) __attribute__((ifunc("resolve_picked")));

FIXTURE_EXPORT int call_picked(void) { return picked(); }
FIXTURE_EXPORT int call_hidden_picked(void) { return hidden_picked() + 1; }
