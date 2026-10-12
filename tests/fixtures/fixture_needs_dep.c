/*
 * AkenoBlox PS5 - loader fixture: DT_NEEDED on libakeno_fixture_dep.so.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fixture_common.h"

extern int dep_value(void);

FIXTURE_EXPORT int needs_dep_value(void) { return dep_value() * 6; }
