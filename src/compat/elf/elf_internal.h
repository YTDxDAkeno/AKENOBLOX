/*
 * AkenoBlox PS5 - akeno_elf internals: the ELF64 structures and checked reads.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The structures are written out here rather than taken from <elf.h> because
 * the console's SDK, glibc and bionic each ship a different <elf.h>, and the
 * Android-specific tags (APS2, RELR before API 30) are in none of the hosts'.
 * Layouts and constants are the System V gABI's, the x86-64 psABI's and
 * bionic's libc/include/elf.h.
 */
#ifndef AKENO_ELF_INTERNAL_H
#define AKENO_ELF_INTERNAL_H

#include "akeno_elf.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct elf64_ehdr {
   uint8_t e_ident[16];
   uint16_t e_type;
   uint16_t e_machine;
   uint32_t e_version;
   uint64_t e_entry;
   uint64_t e_phoff;
   uint64_t e_shoff;
   uint32_t e_flags;
   uint16_t e_ehsize;
   uint16_t e_phentsize;
   uint16_t e_phnum;
   uint16_t e_shentsize;
   uint16_t e_shnum;
   uint16_t e_shstrndx;
} elf64_ehdr;

typedef struct elf64_phdr {
   uint32_t p_type;
   uint32_t p_flags;
   uint64_t p_offset;
   uint64_t p_vaddr;
   uint64_t p_paddr;
   uint64_t p_filesz;
   uint64_t p_memsz;
   uint64_t p_align;
} elf64_phdr;

typedef struct elf64_dyn {
   int64_t d_tag;
   uint64_t d_val;
} elf64_dyn;

typedef struct elf64_sym {
   uint32_t st_name;
   uint8_t st_info;
   uint8_t st_other;
   uint16_t st_shndx;
   uint64_t st_value;
   uint64_t st_size;
} elf64_sym;

typedef struct elf64_rela {
   uint64_t r_offset;
   uint64_t r_info;
   int64_t r_addend;
} elf64_rela;

typedef struct elf64_verneed {
   uint16_t vn_version;
   uint16_t vn_cnt;
   uint32_t vn_file;
   uint32_t vn_aux;
   uint32_t vn_next;
} elf64_verneed;

typedef struct elf64_vernaux {
   uint32_t vna_hash;
   uint16_t vna_flags;
   uint16_t vna_other;
   uint32_t vna_name;
   uint32_t vna_next;
} elf64_vernaux;

_Static_assert(sizeof(elf64_ehdr) == 64, "ELF64 header is 64 bytes");
_Static_assert(sizeof(elf64_phdr) == 56, "ELF64 program header is 56 bytes");
_Static_assert(sizeof(elf64_dyn) == 16, "ELF64 dynamic entry is 16 bytes");
_Static_assert(sizeof(elf64_sym) == 24, "ELF64 symbol is 24 bytes");
_Static_assert(sizeof(elf64_rela) == 24, "ELF64 RELA is 24 bytes");
_Static_assert(sizeof(elf64_verneed) == 16, "Verneed is 16 bytes");
_Static_assert(sizeof(elf64_vernaux) == 16, "Vernaux is 16 bytes");

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ET_DYN 3
#define EM_X86_64 62

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_PHDR 6
#define PT_TLS 7
#define PT_GNU_STACK 0x6474e551
#define PT_GNU_RELRO 0x6474e552

#define DT_NULL 0
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_STRSZ 10
#define DT_SYMENT 11
#define DT_INIT 12
#define DT_FINI 13
#define DT_SONAME 14
#define DT_REL 17
#define DT_TEXTREL 22
#define DT_JMPREL 23
#define DT_BIND_NOW 24
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_FLAGS 30
#define DT_PLTREL 20
#define DT_RELRSZ 35
#define DT_RELR 36
#define DT_RELRENT 37
#define DT_GNU_HASH 0x6ffffef5
#define DT_VERSYM 0x6ffffff0
#define DT_FLAGS_1 0x6ffffffb
#define DT_VERNEED 0x6ffffffe
#define DT_VERNEEDNUM 0x6fffffff
/* bionic libc/include/elf.h */
#define DT_ANDROID_REL 0x6000000f
#define DT_ANDROID_RELSZ 0x60000010
#define DT_ANDROID_RELA 0x60000011
#define DT_ANDROID_RELASZ 0x60000012
#define DT_ANDROID_RELR 0x6fffe000
#define DT_ANDROID_RELRSZ 0x6fffe001
#define DT_ANDROID_RELRENT 0x6fffe003

#define DF_TEXTREL 0x4
#define DF_BIND_NOW 0x8
#define DF_1_NOW 0x1

#define SHN_UNDEF 0
#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STB_GNU_UNIQUE 10
#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_TLS 6
#define STT_GNU_IFUNC 10
#define STV_DEFAULT 0
#define STV_PROTECTED 3

#define ELF64_R_SYM(i) ((uint32_t)((i) >> 32))
#define ELF64_R_TYPE(i) ((uint32_t)((i)&0xffffffffu))
#define ELF64_ST_BIND(i) ((uint8_t)((i) >> 4))
#define ELF64_ST_TYPE(i) ((uint8_t)((i)&0xf))
#define ELF64_ST_VISIBILITY(o) ((uint8_t)((o)&0x3))

/* x86-64 psABI relocation types */
#define R_X86_64_NONE 0
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_COPY 5
#define R_X86_64_GLOB_DAT 6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_RELATIVE 8
#define R_X86_64_32 10
#define R_X86_64_32S 11
#define R_X86_64_DTPMOD64 16
#define R_X86_64_DTPOFF64 17
#define R_X86_64_TPOFF64 18
#define R_X86_64_PC64 24
#define R_X86_64_IRELATIVE 37
#define R_X86_64_RELATIVE64 38

/* bionic libc/private/bionic_asm_note.h */
#define NT_ANDROID_TYPE_IDENT 1

/* A cap on what a bounded walk will visit, so a malformed table is a refusal
 * rather than a hang: libroblox.so's own tables are orders below these. */
#define AKENO_ELF_MAX_DYN_ENTRIES 4096u
#define AKENO_ELF_MAX_SYMBOLS (1u << 24)
#define AKENO_ELF_MAX_VERNEED 4096u
#define AKENO_ELF_MAX_PHNUM 256u

/* A status with its one-line explanation. */
static inline akeno_elf_status
elf_fail(char *err, size_t err_size, akeno_elf_status status, const char *format, ...)
   __attribute__((format(printf, 4, 5)));

static inline akeno_elf_status
elf_fail(char *err, size_t err_size, akeno_elf_status status, const char *format, ...)
{
   if (err != NULL && err_size > 0) {
      va_list args;
      va_start(args, format);
      vsnprintf(err, err_size, format, args);
      va_end(args);
   }
   return status;
}

/* a + b without wrapping. */
static inline bool
elf_add(uint64_t a, uint64_t b, uint64_t *sum)
{
   if (a > UINT64_MAX - b)
      return false;
   *sum = a + b;
   return true;
}

/* [offset, offset + length) lies within [0, size). */
static inline bool
elf_range_ok(uint64_t offset, uint64_t length, uint64_t size)
{
   return offset <= size && length <= size - offset;
}

static inline bool
elf_is_pow2(uint64_t value)
{
   return value != 0 && (value & (value - 1)) == 0;
}

/* The file offset of [vaddr, vaddr + length), when it lies wholly within one
 * PT_LOAD's file-backed bytes. */
bool elf_vaddr_to_offset(const akeno_elf_info *info, uint64_t vaddr, uint64_t length,
                         uint64_t *offset);

/* The number of dynamic symbols, from DT_HASH's nchain or by walking
 * DT_GNU_HASH's chains; reads through `read`, which copies length bytes at a
 * virtual address and returns false when they are not available. */
typedef bool (*elf_read_fn)(const void *ctx, uint64_t vaddr, void *out, size_t length);
bool elf_count_symbols(const akeno_elf_info *info, elf_read_fn read, const void *ctx,
                       uint64_t *count);

/* The version name DT_VERSYM gives symbol index, through DT_VERNEED; NULL for
 * an unversioned or local symbol. */
const char *elf_symbol_version(const akeno_elf_info *info, elf_read_fn read, const void *ctx,
                               const char *(*string)(const void *ctx, uint64_t offset),
                               uint64_t index);

#endif /* AKENO_ELF_INTERNAL_H */
