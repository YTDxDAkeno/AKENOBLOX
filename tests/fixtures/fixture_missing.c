/*
 * AkenoBlox PS5 - loader fixture: imports nothing answers, and a weak one.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fixture_common.h"

extern int akeno_not_provided(void);
extern int akeno_also_missing;
extern int akeno_weak_import(void) __attribute__((weak));

FIXTURE_EXPORT int call_missing(void) { return akeno_not_provided(); }
FIXTURE_EXPORT int read_missing(void) { return akeno_also_missing; }
FIXTURE_EXPORT int weak_is_null(void) { return akeno_weak_import == 0; }
