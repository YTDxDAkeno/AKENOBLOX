/*
 * AkenoBlox PS5 - loader fixture: only a weak import, which binds to null.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fixture_common.h"

extern int akeno_weak_import(void) __attribute__((weak));

FIXTURE_EXPORT int weak_is_null(void) { return akeno_weak_import == 0; }
FIXTURE_EXPORT int weak_call_or(int fallback) { return akeno_weak_import ? akeno_weak_import() : fallback; }
