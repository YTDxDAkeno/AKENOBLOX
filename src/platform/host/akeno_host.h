/*
 * AkenoBlox PS5 - host-only extras: what tests and the host build control
 * that the console has no equivalent for.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef AKENO_HOST_H
#define AKENO_HOST_H

#include "akeno_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Code memory with the kernel's pages, or pretending they are page_size
 * (4096, 16384 or 65536; never smaller than the kernel's). */
const akeno_mem_ops *akeno_host_code_memory(void);
const akeno_mem_ops *akeno_host_code_memory_paged(size_t page_size);

/* Scripted input: a comma-separated list of steps, each a button name
 * (up, down, left, right, cross, circle, square, triangle, l1, r1, l2, r2,
 * l3, r3, options, touchpad), "wait" (one idle frame) or "wait:N". Each button
 * step is pressed for one poll and released for the next, so menus see one
 * edge per step. Returns false on an unknown step. */
bool akeno_host_set_input_script(const char *script);
/* True once every scripted step has been consumed. */
bool akeno_host_input_script_done(void);

/* Writes the last presented frame as a binary PPM. Returns 0 on success. */
int akeno_host_save_last_frame(const char *path);
/* Number of frames presented so far. */
uint64_t akeno_host_presented_frames(void);

/* Where storage lives (default ./akeno-data); must be set before first use. */
void akeno_host_set_storage_root(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_HOST_H */
