/*
 * AkenoBlox PS5 - loader fixture: thread-local storage. Built for
 * x86_64-linux-gnu it has a PT_TLS segment (refused); built for Android API 26
 * the compiler emulates it and the object imports __emutls_get_address
 * instead, which is how Roblox's minSdk-26 engine is expected to look.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fixture_common.h"

static __thread int tls_value = 5;

FIXTURE_EXPORT int get_tls(void) { return tls_value; }
