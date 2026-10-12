/*
 * AkenoBlox PS5 - the runtime's status.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_runtime.h"

#include "akeno_elf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const evidence_names[] = {
   "NOT STARTED", "IN PROGRESS", "HOST VERIFIED", "CROSS-BUILD VERIFIED",
   "HARDWARE VERIFIED", "FAILED", "BLOCKED",
};

const char *
akeno_evidence_name(akeno_evidence e)
{
   return (unsigned)e <= AKENO_EV_BLOCKED ? evidence_names[e] : "?";
}

/* Keep in step with docs/PROJECT_STATE.md and docs/TEST_REPORTS.md. */
static const akeno_stage stages[] = {
   {"Native PS5 launcher (this program)", AKENO_EV_CROSS_BUILD_VERIFIED, true,
    "Builds and packages as a signed title; not yet run on a console."},
   {"ELF loader (akeno_elf)", AKENO_EV_HOST_VERIFIED, true,
    "Loads lld-built Android x86-64 test libraries on a host; the console self-test awaits a run."},
   {"Code memory on the console", AKENO_EV_CROSS_BUILD_VERIFIED, true,
    "ps5platform exec.h regions, 16 KiB pages; the Diagnostics self-test is the hardware check."},
   {"AOSP bionic linker port", AKENO_EV_IN_PROGRESS, true,
    "Cordial's mcpelauncher-linker cross-compiles for the console with 18 errors, all in four "
    "known places (docs/FEASIBILITY.md); not yet ported."},
   {"bionic ABI on the console's libc", AKENO_EV_NOT_STARTED, true,
    "errno, signals, sockets, stat, pthread sizes and epoll differ from FreeBSD's; Cordial forwards "
    "these to glibc, which agrees with bionic where the console's libc does not."},
   {"JNI virtual machine (libjnivm)", AKENO_EV_NOT_STARTED, true, "Not yet cross-built."},
   {"Android framework layer", AKENO_EV_NOT_STARTED, true,
    "GameActivity, ANativeWindow, AAssetManager, ALooper; Cordial's implementation is the reference."},
   {"Graphics: Vulkan through RADV", AKENO_EV_NOT_STARTED, true,
    "Roblox dlopens libvulkan and asks for VK_KHR_android_surface, which becomes VK_KHR_display."},
   {"Audio: OpenSL ES to AudioOut", AKENO_EV_NOT_STARTED, true, "Not started."},
   {"Input: DualSense as an Android gamepad", AKENO_EV_HOST_VERIFIED, true,
    "Mapping and event shaping tested on a host; nothing delivers it to an engine yet."},
   {"Roblox package (user-supplied)", AKENO_EV_IN_PROGRESS, true,
    "The package check validates a libroblox.so without running it."},
   {"Roblox engine start", AKENO_EV_NOT_STARTED, true, "Needs every layer above."},
   {"Sign-in (Quick Sign-in)", AKENO_EV_NOT_STARTED, true,
    "Cordial signs in with Roblox's code flow; whether Roblox accepts this runtime is unknown."},
   {"Joining an experience", AKENO_EV_NOT_STARTED, true, "Not started."},
};

size_t
akeno_runtime_stages(const akeno_stage **out)
{
   *out = stages;
   return sizeof(stages) / sizeof(stages[0]);
}

const akeno_stage *
akeno_runtime_first_blocker(void)
{
   for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); i++)
      if (stages[i].required_for_launch && stages[i].evidence != AKENO_EV_HARDWARE_VERIFIED)
         return &stages[i];
   return NULL;
}

const akeno_stage *
akeno_runtime_next_to_build(void)
{
   for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); i++) {
      const akeno_evidence e = stages[i].evidence;
      if (stages[i].required_for_launch &&
          (e == AKENO_EV_NOT_STARTED || e == AKENO_EV_IN_PROGRESS || e == AKENO_EV_FAILED ||
           e == AKENO_EV_BLOCKED))
         return &stages[i];
   }
   return NULL;
}

size_t
akeno_runtime_awaiting_hardware(void)
{
   size_t count = 0;
   for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); i++)
      count += stages[i].evidence == AKENO_EV_HOST_VERIFIED ||
               stages[i].evidence == AKENO_EV_CROSS_BUILD_VERIFIED;
   return count;
}

/* ----------------------------------------------------- package check */

static bool
count_import(const akeno_elf_import *import, void *ctx)
{
   (void)import;
   (*(long *)ctx)++;
   return true;
}

void
akeno_package_check_file(const char *path, akeno_package_check *c)
{
   memset(c, 0, sizeof(*c));
   c->searched = true;
   snprintf(c->path, sizeof(c->path), "%s", path);
   FILE *f = fopen(path, "rb");
   if (f == NULL) {
      snprintf(c->problem, sizeof(c->problem), "not installed");
      return;
   }
   c->found = true;
   if (fseek(f, 0, SEEK_END) != 0) {
      fclose(f);
      snprintf(c->problem, sizeof(c->problem), "cannot read the file");
      return;
   }
   const long length = ftell(f);
   if (length <= 0 || (unsigned long)length > (UINT64_C(1) << 30) || fseek(f, 0, SEEK_SET) != 0) {
      fclose(f);
      snprintf(c->problem, sizeof(c->problem), "unexpected size %ld bytes", length);
      return;
   }
   c->size = (uint64_t)length;
   uint8_t *image = malloc((size_t)length);
   if (image == NULL) {
      fclose(f);
      snprintf(c->problem, sizeof(c->problem), "no memory for %ld bytes", length);
      return;
   }
   const size_t read = fread(image, 1, (size_t)length, f);
   fclose(f);
   if (read != (size_t)length) {
      free(image);
      snprintf(c->problem, sizeof(c->problem), "short read");
      return;
   }
   akeno_elf_info info;
   char why[160];
   const akeno_elf_status st = akeno_elf_parse(image, (size_t)length, &info, why, sizeof(why));
   c->machine = info.machine;
   if (st != AKENO_ELF_OK) {
      snprintf(c->problem, sizeof(c->problem), "%s: %.120s", akeno_elf_status_name(st), why);
      free(image);
      return;
   }
   c->valid_elf = true;
   c->x86_64 = info.machine == 62;
   c->android_api = info.has_android_note ? info.android_api : 0;
   snprintf(c->ndk, sizeof(c->ndk), "%s", info.ndk_version);
   c->has_tls = info.has_tls;
   c->has_textrel = info.has_textrel;
   c->aps2 = info.relocs.android_rela_bytes > 0;
   c->relr = info.relocs.relr_bytes > 0;
   c->load_segments = info.load_count;
   c->max_align = info.max_align;
   c->needed_count = info.needed_count;
   for (uint32_t i = 0; i < info.needed_count && i < 8 && i < AKENO_ELF_MAX_NEEDED; i++) {
      const char *name = akeno_elf_dynstr(&info, image, (size_t)length, info.needed[i]);
      snprintf(c->needed[i], sizeof(c->needed[i]), "%s", name ? name : "?");
   }
   long counted = 0;
   c->import_count = akeno_elf_for_each_import(&info, image, (size_t)length, count_import, &counted);
   c->imports_answered = 0;
   if (!info.has_android_note)
      snprintf(c->problem, sizeof(c->problem), "no Android ident note: is this Roblox's build?");
   else if (c->has_tls)
      snprintf(c->problem, sizeof(c->problem), "has native TLS; the console emulates TLS");
   free(image);
}

void
akeno_package_check_run(const char *storage_root, akeno_package_check *c)
{
   char path[256];
   snprintf(path, sizeof(path), "%s/roblox/lib/x86_64/libroblox.so", storage_root);
   akeno_package_check_file(path, c);
}
