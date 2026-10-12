/*
 * AkenoBlox PS5 - loader test fixtures: shared definitions.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The fixtures are built freestanding (no libc, no TLS, no stack protector)
 * with an Android x86-64 target, so the same bytes are valid on a Linux host
 * and on the console: their only contact with the outside is the imports the
 * loader's resolver answers. That is what lets the PS5 build embed them as its
 * hardware self-test (src/diagnostics/akeno_selftest.c).
 */
#ifndef AKENO_FIXTURE_COMMON_H
#define AKENO_FIXTURE_COMMON_H

#define FIXTURE_EXPORT __attribute__((visibility("default")))

/* Answered by the host's table (tests and the app's self-test both supply them). */
extern int akeno_host_add(int a, int b);
extern void akeno_host_note(const char *event);

#endif
