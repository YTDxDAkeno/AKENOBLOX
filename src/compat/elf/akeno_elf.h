/*
 * AkenoBlox PS5 - akeno_elf: a bounded ELF64 x86-64 shared-object loader.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why this exists beside the AOSP bionic linker Cordial uses: the console
 * refuses dlopen of anything a title brings and refuses execute permission at
 * map time (PS5_VulkanTemplate skills/ps5-porting, loading-code.md and
 * memory-and-jit.md), so the first thing to prove on hardware is the smallest
 * possible "copy into memory, relocate, make executable, call" path. This
 * module is that proof, the validator the installer runs over a user-supplied
 * libroblox.so before anything executes it, and the reference the bionic
 * linker port is checked against (docs/DECISIONS.md, ADR-0003).
 *
 * Every read of the image is bounds-checked. The image is never executed in
 * place: segments are copied into memory obtained through akeno_mem_ops, so
 * the platform decides how executable memory is made (mmap on a host,
 * ps5platform/exec.h on the console).
 */
#ifndef AKENO_ELF_H
#define AKENO_ELF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "akeno_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum akeno_elf_status {
   AKENO_ELF_OK = 0,
   AKENO_ELF_E_INVALID_ARGUMENT,
   AKENO_ELF_E_TRUNCATED,
   AKENO_ELF_E_BAD_MAGIC,
   AKENO_ELF_E_BAD_CLASS,
   AKENO_ELF_E_BAD_ENCODING,
   AKENO_ELF_E_BAD_VERSION,
   AKENO_ELF_E_BAD_TYPE,
   AKENO_ELF_E_BAD_MACHINE,
   AKENO_ELF_E_BAD_PHDR,
   AKENO_ELF_E_BAD_SEGMENT,
   AKENO_ELF_E_NO_LOAD,
   AKENO_ELF_E_TOO_LARGE,
   AKENO_ELF_E_BAD_DYNAMIC,
   AKENO_ELF_E_BAD_STRTAB,
   AKENO_ELF_E_BAD_SYMTAB,
   AKENO_ELF_E_BAD_HASH,
   AKENO_ELF_E_BAD_RELOC,
   AKENO_ELF_E_UNSUPPORTED_RELOC,
   AKENO_ELF_E_UNDEFINED_SYMBOL,
   AKENO_ELF_E_TLS_UNSUPPORTED,
   AKENO_ELF_E_IFUNC_REFUSED,
   AKENO_ELF_E_NO_MEMORY,
   AKENO_ELF_E_PROTECT_FAILED,
   AKENO_ELF_E_STATE,
   AKENO_ELF_STATUS_COUNT
} akeno_elf_status;

/* A stable name for logs and tests, e.g. "E_UNDEFINED_SYMBOL". */
const char *akeno_elf_status_name(akeno_elf_status status);

/* ------------------------------------------------------------ inspection */

enum {
   AKENO_ELF_MAX_LOAD = 16,
   AKENO_ELF_MAX_NEEDED = 64,
   AKENO_ELF_NDK_VERSION_BYTES = 64
};

/* Segment permission bits, the ELF PF_* values. */
#define AKENO_PF_X 0x1u
#define AKENO_PF_W 0x2u
#define AKENO_PF_R 0x4u

typedef struct akeno_elf_segment {
   uint64_t vaddr;
   uint64_t memsz;
   uint64_t offset;
   uint64_t filesz;
   uint64_t align;
   uint32_t flags; /* AKENO_PF_* */
} akeno_elf_segment;

/* The relocation encodings a library may carry. Android adds two packed forms
 * to plain RELA: APS2 (DT_ANDROID_RELA, "packed relocations", API 23+) and
 * RELR (DT_RELR, or DT_ANDROID_RELR before API 30). Cordial measured that
 * libroblox.so has no plain .rela.dyn at all, only 546 .rela.plt entries
 * (cordial patches/README.md, patch 0001), so its relative relocations are
 * packed and the loader must decode both forms. */
typedef struct akeno_elf_reloc_summary {
   uint64_t rela_bytes;        /* DT_RELA */
   uint64_t plt_bytes;         /* DT_JMPREL */
   uint64_t android_rela_bytes; /* DT_ANDROID_RELA (APS2) */
   uint64_t relr_bytes;        /* DT_RELR or DT_ANDROID_RELR */
   bool has_rel;               /* DT_REL / DT_ANDROID_REL: refused on x86-64 */
} akeno_elf_reloc_summary;

typedef struct akeno_elf_info {
   uint16_t type;    /* ET_* */
   uint16_t machine; /* EM_* */
   uint8_t osabi;
   uint64_t entry;

   uint32_t load_count;
   akeno_elf_segment load[AKENO_ELF_MAX_LOAD];
   uint64_t min_vaddr; /* lowest PT_LOAD p_vaddr */
   uint64_t max_vaddr; /* highest PT_LOAD p_vaddr + p_memsz */
   uint64_t max_align; /* largest PT_LOAD p_align */
   uint32_t load_flags_union;

   bool has_dynamic;
   bool has_interp;
   bool has_tls;
   bool has_textrel;
   bool has_gnu_hash;
   bool has_sysv_hash;
   bool has_relro;
   bool has_init;
   bool has_fini;
   bool bind_now;
   bool stack_executable;
   uint64_t tls_memsz;
   uint64_t relro_vaddr, relro_memsz;
   uint64_t init_array_count, fini_array_count;

   akeno_elf_reloc_summary relocs;

   uint32_t needed_count;          /* may exceed AKENO_ELF_MAX_NEEDED; only that many kept */
   uint64_t needed[AKENO_ELF_MAX_NEEDED]; /* offsets into the dynamic string table */
   bool has_soname;
   uint64_t soname;                /* offset into the dynamic string table */

   /* The Android ident note (NT_ANDROID_TYPE_IDENT): the API level the object
    * was built for and, from NDK r14, the NDK's version string. */
   bool has_android_note;
   uint32_t android_api;
   char ndk_version[AKENO_ELF_NDK_VERSION_BYTES];

   /* Dynamic table locations, as virtual addresses (0 when absent). */
   uint64_t dyn_vaddr, dyn_count;
   uint64_t strtab_vaddr, strtab_size;
   uint64_t symtab_vaddr, symtab_entsize, symbol_count;
   uint64_t hash_vaddr, gnu_hash_vaddr;
   uint64_t rela_vaddr, jmprel_vaddr, android_rela_vaddr, relr_vaddr;
   uint64_t versym_vaddr, verneed_vaddr, verneed_count;
   uint64_t init_vaddr, fini_vaddr;
   uint64_t init_array_vaddr, fini_array_vaddr;
} akeno_elf_info;

/* Validates and summarises an ELF64 little-endian x86-64 shared object held
 * in memory. Nothing is mapped or executed. err, when given, receives a
 * one-line reason on failure. */
akeno_elf_status akeno_elf_parse(const void *image, size_t size, akeno_elf_info *info,
                                 char *err, size_t err_size);

/* A string from the dynamic string table, read from the file image (NULL when
 * the offset or the string is out of bounds). */
const char *akeno_elf_dynstr(const akeno_elf_info *info, const void *image, size_t size,
                             uint64_t offset);

/* An imported (undefined) dynamic symbol. */
typedef struct akeno_elf_import {
   const char *name;
   const char *version; /* from DT_VERNEED, or NULL */
   uint8_t bind;        /* STB_* */
   uint8_t type;        /* STT_* */
} akeno_elf_import;

/* Calls fn for every undefined dynamic symbol with a name, in table order,
 * until fn returns false. Returns the number visited, or -1 when the tables
 * are malformed. This is how a missing-symbol report is made before anything
 * is loaded (Cordial ADR-034: resolution asks the library what it imports). */
long akeno_elf_for_each_import(const akeno_elf_info *info, const void *image, size_t size,
                               bool (*fn)(const akeno_elf_import *import, void *ctx), void *ctx);

/* --------------------------------------------------------------- loading */

/* Answers an import. Returns its address, or NULL when nothing provides it.
 * version is the DT_VERNEED name or NULL. A resolver must never return a
 * stub that pretends to succeed: an unimplemented import is NULL, and the
 * load fails naming it (Cordial AGENTS.md, "Never make a stub lie"). */
typedef void *(*akeno_elf_resolve_fn)(void *ctx, const char *name, const char *version);

typedef struct akeno_elf_load_options {
   const akeno_mem_ops *mem; /* required */
   akeno_elf_resolve_fn resolve;
   void *resolve_ctx;
   /* Run DT_INIT and DT_INIT_ARRAY after loading (and DT_FINI_ARRAY, DT_FINI
    * at unload). Off for validation and fuzzing. */
   bool run_constructors;
   /* IRELATIVE and STT_GNU_IFUNC call code inside the object during loading;
    * refused unless allowed. */
   bool allow_ifunc;
   /* Refuse images whose mapped span exceeds this (0: 2 GiB). */
   uint64_t max_span;
   /* What bionic hands DT_INIT_ARRAY functions. */
   int argc;
   char **argv;
   char **envp;
} akeno_elf_load_options;

typedef struct akeno_elf_module akeno_elf_module;

/* Missing imports are collected, not stopped at: a failed load names up to
 * AKENO_ELF_MISSING_KEPT of them, and counts all of them. */
enum { AKENO_ELF_MISSING_KEPT = 16, AKENO_ELF_NAME_BYTES = 96 };

typedef struct akeno_elf_load_report {
   akeno_elf_status status;
   uint64_t relocations_applied;
   uint64_t relative_applied;   /* R_X86_64_RELATIVE and RELR entries */
   uint64_t symbolic_applied;   /* relocations that needed a symbol */
   uint64_t deferred_applied;   /* IRELATIVE and IFUNC */
   uint32_t missing_count;
   char missing[AKENO_ELF_MISSING_KEPT][AKENO_ELF_NAME_BYTES];
   uint32_t wx_pages;           /* pages that had to be writable and executable */
   unsigned applied_union;      /* union of protections the platform applied */
   char message[192];
} akeno_elf_load_report;

/* Copies the image into memory from opt->mem, relocates it, applies segment
 * protections and (optionally) runs constructors. The image buffer may be
 * freed afterwards. report is optional. */
akeno_elf_status akeno_elf_load(const void *image, size_t size, const akeno_elf_load_options *opt,
                                akeno_elf_module **module, akeno_elf_load_report *report);

/* The address of a defined, exported dynamic symbol, or NULL. */
void *akeno_elf_symbol(const akeno_elf_module *module, const char *name);

/* Base (load bias) and mapped size of a loaded module. */
uintptr_t akeno_elf_base(const akeno_elf_module *module);
size_t akeno_elf_span(const akeno_elf_module *module);
/* The protection the platform applied to the page containing address, or
 * AKENO_PROT_NONE when it is outside the module. */
unsigned akeno_elf_page_protection(const akeno_elf_module *module, const void *address);

/* Runs DT_FINI_ARRAY in reverse and DT_FINI (when constructors ran), then
 * releases the memory. NULL is ignored. */
void akeno_elf_unload(akeno_elf_module *module);

/* ------------------------------------------------------- symbol tables */

/* A sorted table of name -> address: how a set of host functions is offered
 * to an engine as a library (Cordial's "virtual libraries"). */
typedef struct akeno_symbol {
   const char *name;
   void *address;
} akeno_symbol;

typedef struct akeno_symbol_table {
   const char *library; /* the soname it stands in for */
   akeno_symbol *symbols;
   size_t count;
} akeno_symbol_table;

/* Sorts the table in place; returns false when a name appears twice. */
bool akeno_symbol_table_seal(akeno_symbol_table *table);
void *akeno_symbol_table_find(const akeno_symbol_table *table, const char *name);

/* A resolver chain: loaded modules' exports first, then tables, in order. */
enum { AKENO_RESOLVER_MAX = 16 };
typedef struct akeno_resolver_chain {
   const akeno_elf_module *modules[AKENO_RESOLVER_MAX];
   size_t module_count;
   const akeno_symbol_table *tables[AKENO_RESOLVER_MAX];
   size_t table_count;
} akeno_resolver_chain;

/* An akeno_elf_resolve_fn over an akeno_resolver_chain. */
void *akeno_resolver_chain_resolve(void *chain, const char *name, const char *version);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_ELF_H */
