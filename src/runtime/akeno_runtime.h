/*
 * AkenoBlox PS5 - the runtime's status: which layers between this program
 * and Roblox's engine exist, and what is known about each.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * "Start Roblox" reads this and says plainly why the engine cannot start yet,
 * naming the first layer that blocks it. Nothing here is set at run time to a
 * better state than the evidence in docs/TEST_REPORTS.md supports; a stage
 * moves when its acceptance test passes, in the commit that records the test.
 */
#ifndef AKENO_RUNTIME_H
#define AKENO_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The verification vocabulary of docs/TEST_REPORTS.md. */
typedef enum akeno_evidence {
   AKENO_EV_NOT_STARTED = 0,
   AKENO_EV_IN_PROGRESS,
   AKENO_EV_HOST_VERIFIED,
   AKENO_EV_CROSS_BUILD_VERIFIED,
   AKENO_EV_HARDWARE_VERIFIED,
   AKENO_EV_FAILED,
   AKENO_EV_BLOCKED,
} akeno_evidence;

typedef struct akeno_stage {
   const char *name;
   akeno_evidence evidence;
   bool required_for_launch;
   const char *note;
} akeno_stage;

const char *akeno_evidence_name(akeno_evidence evidence);

/* The stages, in dependency order. */
size_t akeno_runtime_stages(const akeno_stage **stages);
/* The first required stage not hardware-verified, or NULL when none is left. */
const akeno_stage *akeno_runtime_first_blocker(void);
/* The first required stage that still needs building: not started, in
 * progress, failed or blocked. This is why Roblox cannot start. */
const akeno_stage *akeno_runtime_next_to_build(void);
/* Stages built and verified on a host or by a cross-build, but not yet on a
 * console. */
size_t akeno_runtime_awaiting_hardware(void);

/* ---------------------------------------- the user-supplied engine */

typedef struct akeno_package_check {
   bool searched;
   bool found;
   char path[256];
   uint64_t size;
   bool valid_elf;
   char problem[160];     /* why it is not usable, when it is not */
   uint16_t machine;
   bool x86_64;
   uint32_t android_api;
   char ndk[64];
   uint32_t needed_count;
   char needed[8][40];
   long import_count;
   long imports_answered; /* by tables this build links: 0 until the bionic shim lands */
   bool has_tls;
   bool has_textrel;
   bool aps2, relr;
   uint32_t load_segments;
   uint64_t max_align;
} akeno_package_check;

/* Looks for <storage>/roblox/lib/x86_64/libroblox.so, the layout the
 * installer will write (docs/INSTALLATION.md), and validates it with the ELF
 * parser. Nothing is mapped or executed. The file is the user's own copy of
 * Roblox's Android build; this program never downloads or ships it. */
void akeno_package_check_run(const char *storage_root, akeno_package_check *check);

/* The same, for a file at an explicit path (tests, and a future installer). */
void akeno_package_check_file(const char *path, akeno_package_check *check);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_RUNTIME_H */
