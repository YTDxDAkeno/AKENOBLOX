/*
 * AkenoBlox PS5 - libFuzzer target: the ELF parser and the import walk.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   cmake -DAKENO_FUZZ=ON -DCMAKE_C_COMPILER=clang ...; ./fuzz_elf_parse corpus/
 * Seed the corpus with the fixtures in build/fixtures.
 */
#include "akeno_elf.h"

#include <stddef.h>
#include <stdint.h>

static bool
visit(const akeno_elf_import *import, void *ctx)
{
   (void)ctx;
   return import->name[0] != '\0';
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
   akeno_elf_info info;
   char err[160];
   if (akeno_elf_parse(data, size, &info, err, sizeof(err)) == AKENO_ELF_OK) {
      akeno_elf_for_each_import(&info, data, size, visit, NULL);
      for (uint32_t i = 0; i < info.needed_count && i < AKENO_ELF_MAX_NEEDED; i++)
         (void)akeno_elf_dynstr(&info, data, size, info.needed[i]);
   }
   return 0;
}
