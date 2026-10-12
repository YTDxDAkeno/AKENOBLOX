/*
 * AkenoBlox PS5 - libFuzzer target: loading (copy, relocate, protect), with
 * constructors and IFUNC resolvers off, so no fuzzed code ever runs.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_elf.h"
#include "akeno_host.h"

#include <stddef.h>
#include <stdint.h>

static int fuzz_target(void) { return 0; }

static void *
resolve(void *ctx, const char *name, const char *version)
{
   (void)ctx;
   (void)version;
   return name[0] == 'a' ? (void *)fuzz_target : NULL;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
   akeno_elf_load_options opt = {
      .mem = akeno_host_code_memory(),
      .resolve = resolve,
      .run_constructors = false,
      .allow_ifunc = false,
      .max_span = 64u << 20,
   };
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   if (akeno_elf_load(data, size, &opt, &m, &report) == AKENO_ELF_OK) {
      (void)akeno_elf_symbol(m, "fixture_add");
      (void)akeno_elf_symbol(m, "");
      akeno_elf_unload(m);
   }
   return 0;
}
