/*
 * AkenoBlox PS5 - akeno_elf: validating an image and reading its tables.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The parser never trusts a size or an offset from the file: each table is
 * located through the PT_LOAD that contains it and read only when wholly
 * inside the bytes the file supplies. It is what the installer runs over a
 * user-supplied libroblox.so before anything maps it, so a malformed or
 * hostile file is a refusal with a reason, never a fault (tests/fuzz).
 */
#include "elf_internal.h"

#include <stdlib.h>

static const char *const status_names[AKENO_ELF_STATUS_COUNT] = {
   [AKENO_ELF_OK] = "OK",
   [AKENO_ELF_E_INVALID_ARGUMENT] = "E_INVALID_ARGUMENT",
   [AKENO_ELF_E_TRUNCATED] = "E_TRUNCATED",
   [AKENO_ELF_E_BAD_MAGIC] = "E_BAD_MAGIC",
   [AKENO_ELF_E_BAD_CLASS] = "E_BAD_CLASS",
   [AKENO_ELF_E_BAD_ENCODING] = "E_BAD_ENCODING",
   [AKENO_ELF_E_BAD_VERSION] = "E_BAD_VERSION",
   [AKENO_ELF_E_BAD_TYPE] = "E_BAD_TYPE",
   [AKENO_ELF_E_BAD_MACHINE] = "E_BAD_MACHINE",
   [AKENO_ELF_E_BAD_PHDR] = "E_BAD_PHDR",
   [AKENO_ELF_E_BAD_SEGMENT] = "E_BAD_SEGMENT",
   [AKENO_ELF_E_NO_LOAD] = "E_NO_LOAD",
   [AKENO_ELF_E_TOO_LARGE] = "E_TOO_LARGE",
   [AKENO_ELF_E_BAD_DYNAMIC] = "E_BAD_DYNAMIC",
   [AKENO_ELF_E_BAD_STRTAB] = "E_BAD_STRTAB",
   [AKENO_ELF_E_BAD_SYMTAB] = "E_BAD_SYMTAB",
   [AKENO_ELF_E_BAD_HASH] = "E_BAD_HASH",
   [AKENO_ELF_E_BAD_RELOC] = "E_BAD_RELOC",
   [AKENO_ELF_E_UNSUPPORTED_RELOC] = "E_UNSUPPORTED_RELOC",
   [AKENO_ELF_E_UNDEFINED_SYMBOL] = "E_UNDEFINED_SYMBOL",
   [AKENO_ELF_E_TLS_UNSUPPORTED] = "E_TLS_UNSUPPORTED",
   [AKENO_ELF_E_IFUNC_REFUSED] = "E_IFUNC_REFUSED",
   [AKENO_ELF_E_NO_MEMORY] = "E_NO_MEMORY",
   [AKENO_ELF_E_PROTECT_FAILED] = "E_PROTECT_FAILED",
   [AKENO_ELF_E_STATE] = "E_STATE",
};

const char *
akeno_elf_status_name(akeno_elf_status status)
{
   if ((unsigned)status >= AKENO_ELF_STATUS_COUNT || status_names[status] == NULL)
      return "E_UNKNOWN";
   return status_names[status];
}

bool
elf_vaddr_to_offset(const akeno_elf_info *info, uint64_t vaddr, uint64_t length, uint64_t *offset)
{
   for (uint32_t i = 0; i < info->load_count; i++) {
      const akeno_elf_segment *s = &info->load[i];
      if (vaddr < s->vaddr)
         continue;
      const uint64_t into = vaddr - s->vaddr;
      if (into > s->filesz || length > s->filesz - into)
         continue;
      *offset = s->offset + into;
      return true;
   }
   return false;
}

/* [vaddr, vaddr + length) lies within one PT_LOAD's memory image. */
static bool
vaddr_in_memory(const akeno_elf_info *info, uint64_t vaddr, uint64_t length)
{
   for (uint32_t i = 0; i < info->load_count; i++) {
      const akeno_elf_segment *s = &info->load[i];
      if (vaddr < s->vaddr)
         continue;
      const uint64_t into = vaddr - s->vaddr;
      if (into <= s->memsz && length <= s->memsz - into)
         return true;
   }
   return false;
}

struct file_view {
   const akeno_elf_info *info;
   const uint8_t *bytes;
   size_t size;
};

static bool
file_read(const void *ctx, uint64_t vaddr, void *out, size_t length)
{
   const struct file_view *view = ctx;
   uint64_t offset;
   if (!elf_vaddr_to_offset(view->info, vaddr, length, &offset))
      return false;
   if (!elf_range_ok(offset, length, view->size))
      return false;
   memcpy(out, view->bytes + offset, length);
   return true;
}

static const char *
file_string(const void *ctx, uint64_t offset)
{
   const struct file_view *view = ctx;
   return akeno_elf_dynstr(view->info, view->bytes, view->size, offset);
}

const char *
akeno_elf_dynstr(const akeno_elf_info *info, const void *image, size_t size, uint64_t offset)
{
   if (info == NULL || image == NULL || info->strtab_size == 0 || offset >= info->strtab_size)
      return NULL;
   uint64_t table;
   if (!elf_vaddr_to_offset(info, info->strtab_vaddr, info->strtab_size, &table))
      return NULL;
   if (!elf_range_ok(table, info->strtab_size, size))
      return NULL;
   const char *start = (const char *)image + table + offset;
   if (memchr(start, '\0', (size_t)(info->strtab_size - offset)) == NULL)
      return NULL;
   return start;
}

bool
elf_count_symbols(const akeno_elf_info *info, elf_read_fn read, const void *ctx, uint64_t *count)
{
   if (info->hash_vaddr != 0) {
      uint32_t header[2];
      if (!read(ctx, info->hash_vaddr, header, sizeof(header)))
         return false;
      /* nbucket, nchain: the chain array has one entry per symbol. */
      if (header[1] > AKENO_ELF_MAX_SYMBOLS)
         return false;
      *count = header[1];
      return true;
   }
   if (info->gnu_hash_vaddr == 0)
      return false;
   uint32_t header[4];
   if (!read(ctx, info->gnu_hash_vaddr, header, sizeof(header)))
      return false;
   const uint32_t nbuckets = header[0], symoffset = header[1], bloom_size = header[2];
   if (nbuckets == 0 || nbuckets > AKENO_ELF_MAX_SYMBOLS || bloom_size > AKENO_ELF_MAX_SYMBOLS ||
       symoffset > AKENO_ELF_MAX_SYMBOLS)
      return false;
   const uint64_t buckets = info->gnu_hash_vaddr + 16 + (uint64_t)bloom_size * 8;
   uint32_t highest = 0;
   for (uint32_t b = 0; b < nbuckets; b++) {
      uint32_t value;
      if (!read(ctx, buckets + (uint64_t)b * 4, &value, sizeof(value)))
         return false;
      if (value > highest)
         highest = value;
   }
   if (highest < symoffset) {
      *count = symoffset;
      return true;
   }
   /* The highest bucket's chain ends at the last symbol. */
   const uint64_t chain = buckets + (uint64_t)nbuckets * 4;
   for (uint64_t index = highest; index < AKENO_ELF_MAX_SYMBOLS; index++) {
      uint32_t hash;
      if (!read(ctx, chain + (index - symoffset) * 4, &hash, sizeof(hash)))
         return false;
      if (hash & 1u) {
         *count = index + 1;
         return true;
      }
   }
   return false;
}

const char *
elf_symbol_version(const akeno_elf_info *info, elf_read_fn read, const void *ctx,
                   const char *(*string)(const void *ctx, uint64_t offset), uint64_t index)
{
   if (info->versym_vaddr == 0 || info->verneed_vaddr == 0)
      return NULL;
   uint16_t versym;
   if (!read(ctx, info->versym_vaddr + index * 2, &versym, sizeof(versym)))
      return NULL;
   const uint16_t wanted = versym & 0x7fffu;
   if (wanted <= 1) /* local, or global unversioned */
      return NULL;
   uint64_t entry = info->verneed_vaddr;
   const uint64_t entries = info->verneed_count ? info->verneed_count : AKENO_ELF_MAX_VERNEED;
   for (uint64_t n = 0; n < entries && n < AKENO_ELF_MAX_VERNEED; n++) {
      elf64_verneed need;
      if (!read(ctx, entry, &need, sizeof(need)))
         return NULL;
      uint64_t aux = entry + need.vn_aux;
      for (uint16_t a = 0; a < need.vn_cnt; a++) {
         elf64_vernaux vernaux;
         if (!read(ctx, aux, &vernaux, sizeof(vernaux)))
            return NULL;
         if (vernaux.vna_other == wanted)
            return string(ctx, vernaux.vna_name);
         if (vernaux.vna_next == 0)
            break;
         aux += vernaux.vna_next;
      }
      if (need.vn_next == 0)
         break;
      entry += need.vn_next;
   }
   return NULL;
}

/* The Android ident note: namesz 8 ("Android\0"), type 1, then the API level
 * and, from NDK r14, 64 bytes of NDK version and 64 of build number. */
static void
parse_notes(akeno_elf_info *info, const uint8_t *bytes, uint64_t offset, uint64_t length)
{
   uint64_t at = 0;
   while (length - at >= 12) {
      uint32_t namesz, descsz, type;
      memcpy(&namesz, bytes + offset + at, 4);
      memcpy(&descsz, bytes + offset + at + 4, 4);
      memcpy(&type, bytes + offset + at + 8, 4);
      const uint64_t name_padded = ((uint64_t)namesz + 3u) & ~(uint64_t)3u;
      const uint64_t desc_padded = ((uint64_t)descsz + 3u) & ~(uint64_t)3u;
      const uint64_t body = at + 12;
      if (name_padded > length - body || desc_padded > length - body - name_padded)
         return;
      const uint8_t *name = bytes + offset + body;
      const uint8_t *desc = name + name_padded;
      if (type == NT_ANDROID_TYPE_IDENT && namesz == 8 && memcmp(name, "Android", 8) == 0 &&
          descsz >= 4) {
         info->has_android_note = true;
         memcpy(&info->android_api, desc, 4);
         if (descsz >= 4 + AKENO_ELF_NDK_VERSION_BYTES) {
            memcpy(info->ndk_version, desc + 4, AKENO_ELF_NDK_VERSION_BYTES);
            info->ndk_version[AKENO_ELF_NDK_VERSION_BYTES - 1] = '\0';
         }
      }
      at = body + name_padded + desc_padded;
   }
}

static akeno_elf_status
parse_dynamic(akeno_elf_info *info, const uint8_t *bytes, size_t size, char *err, size_t err_size)
{
   uint64_t offset;
   const uint64_t length = info->dyn_count * sizeof(elf64_dyn);
   if (!elf_vaddr_to_offset(info, info->dyn_vaddr, length, &offset) ||
       !elf_range_ok(offset, length, size))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_DYNAMIC,
                      "PT_DYNAMIC at 0x%llx is not inside the file's loaded bytes",
                      (unsigned long long)info->dyn_vaddr);

   uint64_t relasz = 0, relaent = sizeof(elf64_rela), pltrel = DT_RELA, pltrelsz = 0;
   uint64_t android_relasz = 0, relrsz = 0, relrent = 8, init_arraysz = 0, fini_arraysz = 0;
   uint64_t symtab_entsize = sizeof(elf64_sym), flags = 0, flags_1 = 0;
   bool saw_null = false;

   for (uint64_t i = 0; i < info->dyn_count; i++) {
      elf64_dyn d;
      memcpy(&d, bytes + offset + i * sizeof(d), sizeof(d));
      switch (d.d_tag) {
      case DT_NULL: saw_null = true; break;
      case DT_NEEDED:
         if (info->needed_count < AKENO_ELF_MAX_NEEDED)
            info->needed[info->needed_count] = d.d_val;
         info->needed_count++;
         break;
      case DT_SONAME: info->has_soname = true; info->soname = d.d_val; break;
      case DT_STRTAB: info->strtab_vaddr = d.d_val; break;
      case DT_STRSZ: info->strtab_size = d.d_val; break;
      case DT_SYMTAB: info->symtab_vaddr = d.d_val; break;
      case DT_SYMENT: symtab_entsize = d.d_val; break;
      case DT_HASH: info->hash_vaddr = d.d_val; info->has_sysv_hash = true; break;
      case DT_GNU_HASH: info->gnu_hash_vaddr = d.d_val; info->has_gnu_hash = true; break;
      case DT_RELA: info->rela_vaddr = d.d_val; break;
      case DT_RELASZ: relasz = d.d_val; break;
      case DT_RELAENT: relaent = d.d_val; break;
      case DT_REL:
      case DT_ANDROID_REL: info->relocs.has_rel = true; break;
      case DT_JMPREL: info->jmprel_vaddr = d.d_val; break;
      case DT_PLTRELSZ: pltrelsz = d.d_val; break;
      case DT_PLTREL: pltrel = d.d_val; break;
      case DT_ANDROID_RELA: info->android_rela_vaddr = d.d_val; break;
      case DT_ANDROID_RELASZ: android_relasz = d.d_val; break;
      case DT_RELR:
      case DT_ANDROID_RELR: info->relr_vaddr = d.d_val; break;
      case DT_RELRSZ:
      case DT_ANDROID_RELRSZ: relrsz = d.d_val; break;
      case DT_RELRENT:
      case DT_ANDROID_RELRENT: relrent = d.d_val; break;
      case DT_INIT: info->init_vaddr = d.d_val; info->has_init = d.d_val != 0; break;
      case DT_FINI: info->fini_vaddr = d.d_val; info->has_fini = d.d_val != 0; break;
      case DT_INIT_ARRAY: info->init_array_vaddr = d.d_val; break;
      case DT_INIT_ARRAYSZ: init_arraysz = d.d_val; break;
      case DT_FINI_ARRAY: info->fini_array_vaddr = d.d_val; break;
      case DT_FINI_ARRAYSZ: fini_arraysz = d.d_val; break;
      case DT_TEXTREL: info->has_textrel = true; break;
      case DT_BIND_NOW: info->bind_now = true; break;
      case DT_FLAGS: flags = d.d_val; break;
      case DT_FLAGS_1: flags_1 = d.d_val; break;
      case DT_VERSYM: info->versym_vaddr = d.d_val; break;
      case DT_VERNEED: info->verneed_vaddr = d.d_val; break;
      case DT_VERNEEDNUM: info->verneed_count = d.d_val; break;
      default: break;
      }
      if (saw_null)
         break;
   }
   if (!saw_null)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_DYNAMIC, "the dynamic section has no DT_NULL");
   if (flags & DF_TEXTREL)
      info->has_textrel = true;
   if ((flags & DF_BIND_NOW) || (flags_1 & DF_1_NOW))
      info->bind_now = true;

   /* The string table: present, inside the file, ending in a NUL. */
   if (info->strtab_vaddr == 0 || info->strtab_size == 0)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_STRTAB, "no DT_STRTAB/DT_STRSZ");
   uint64_t strtab;
   if (!elf_vaddr_to_offset(info, info->strtab_vaddr, info->strtab_size, &strtab) ||
       !elf_range_ok(strtab, info->strtab_size, size))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_STRTAB,
                      "the dynamic string table lies outside the file");
   if (bytes[strtab + info->strtab_size - 1] != '\0')
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_STRTAB,
                      "the dynamic string table does not end in a NUL");
   for (uint32_t i = 0; i < info->needed_count && i < AKENO_ELF_MAX_NEEDED; i++)
      if (info->needed[i] >= info->strtab_size)
         return elf_fail(err, err_size, AKENO_ELF_E_BAD_STRTAB, "a DT_NEEDED name is out of range");
   if (info->has_soname && info->soname >= info->strtab_size)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_STRTAB, "DT_SONAME is out of range");

   /* The symbol table, and its size from the hash table. */
   if (info->symtab_vaddr == 0)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_SYMTAB, "no DT_SYMTAB");
   if (symtab_entsize != sizeof(elf64_sym))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_SYMTAB, "DT_SYMENT is %llu, not 24",
                      (unsigned long long)symtab_entsize);
   info->symtab_entsize = symtab_entsize;
   if (!info->has_gnu_hash && !info->has_sysv_hash)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_HASH, "neither DT_HASH nor DT_GNU_HASH");
   const struct file_view view = {info, bytes, size};
   uint64_t count;
   if (!elf_count_symbols(info, file_read, &view, &count))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_HASH, "the hash table is malformed");
   uint64_t symtab_offset;
   if (count > AKENO_ELF_MAX_SYMBOLS ||
       !elf_vaddr_to_offset(info, info->symtab_vaddr, count * sizeof(elf64_sym), &symtab_offset) ||
       !elf_range_ok(symtab_offset, count * sizeof(elf64_sym), size))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_SYMTAB,
                      "%llu symbols do not fit the file at 0x%llx", (unsigned long long)count,
                      (unsigned long long)info->symtab_vaddr);
   info->symbol_count = count;
   if (info->versym_vaddr != 0) {
      uint64_t versym_offset;
      if (!elf_vaddr_to_offset(info, info->versym_vaddr, count * 2, &versym_offset))
         return elf_fail(err, err_size, AKENO_ELF_E_BAD_SYMTAB, "DT_VERSYM lies outside the file");
   }

   /* Relocation tables are read from the file before anything is mapped. */
   struct {
      uint64_t vaddr, bytes;
      const char *name;
   } tables[] = {
      {info->rela_vaddr, relasz, "DT_RELA"},
      {info->jmprel_vaddr, pltrelsz, "DT_JMPREL"},
      {info->android_rela_vaddr, android_relasz, "DT_ANDROID_RELA"},
      {info->relr_vaddr, relrsz, "DT_RELR"},
   };
   for (size_t t = 0; t < sizeof(tables) / sizeof(tables[0]); t++) {
      if (tables[t].bytes == 0)
         continue;
      uint64_t table_offset;
      if (tables[t].vaddr == 0 ||
          !elf_vaddr_to_offset(info, tables[t].vaddr, tables[t].bytes, &table_offset) ||
          !elf_range_ok(table_offset, tables[t].bytes, size))
         return elf_fail(err, err_size, AKENO_ELF_E_BAD_RELOC, "%s lies outside the file",
                         tables[t].name);
   }
   if (relasz != 0 && (relaent != sizeof(elf64_rela) || relasz % sizeof(elf64_rela) != 0))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_RELOC, "DT_RELASZ/DT_RELAENT are inconsistent");
   if (pltrelsz != 0 && (pltrel != DT_RELA || pltrelsz % sizeof(elf64_rela) != 0))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_RELOC,
                      "DT_JMPREL must be RELA with whole entries on x86-64");
   if (relrsz != 0 && (relrent != 8 || relrsz % 8 != 0))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_RELOC, "DT_RELRSZ/DT_RELRENT are inconsistent");
   info->relocs.rela_bytes = relasz;
   info->relocs.plt_bytes = pltrelsz;
   info->relocs.android_rela_bytes = android_relasz;
   info->relocs.relr_bytes = relrsz;

   /* Constructor and destructor arrays: whole pointers, inside the image. */
   if ((init_arraysz % 8) != 0 || (fini_arraysz % 8) != 0)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_DYNAMIC, "a DT_*_ARRAYSZ is not a multiple of 8");
   if (init_arraysz != 0 &&
       (info->init_array_vaddr == 0 || !vaddr_in_memory(info, info->init_array_vaddr, init_arraysz)))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_DYNAMIC, "DT_INIT_ARRAY lies outside the image");
   if (fini_arraysz != 0 &&
       (info->fini_array_vaddr == 0 || !vaddr_in_memory(info, info->fini_array_vaddr, fini_arraysz)))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_DYNAMIC, "DT_FINI_ARRAY lies outside the image");
   info->init_array_count = init_arraysz / 8;
   info->fini_array_count = fini_arraysz / 8;
   if (info->has_init && !vaddr_in_memory(info, info->init_vaddr, 1))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_DYNAMIC, "DT_INIT lies outside the image");
   if (info->has_fini && !vaddr_in_memory(info, info->fini_vaddr, 1))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_DYNAMIC, "DT_FINI lies outside the image");
   return AKENO_ELF_OK;
}

akeno_elf_status
akeno_elf_parse(const void *image, size_t size, akeno_elf_info *info, char *err, size_t err_size)
{
   if (err != NULL && err_size > 0)
      err[0] = '\0';
   if (image == NULL || info == NULL)
      return elf_fail(err, err_size, AKENO_ELF_E_INVALID_ARGUMENT, "no image or no info");
   memset(info, 0, sizeof(*info));
   const uint8_t *bytes = image;
   if (size < sizeof(elf64_ehdr))
      return elf_fail(err, err_size, AKENO_ELF_E_TRUNCATED, "%zu bytes is smaller than an ELF header",
                      size);
   elf64_ehdr eh;
   memcpy(&eh, bytes, sizeof(eh));
   if (memcmp(eh.e_ident, "\177ELF", 4) != 0)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_MAGIC, "not an ELF file");
   if (eh.e_ident[4] != ELFCLASS64)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_CLASS, "ELF class %u, not 64-bit",
                      eh.e_ident[4]);
   if (eh.e_ident[5] != ELFDATA2LSB)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_ENCODING, "not little-endian");
   if (eh.e_ident[6] != EV_CURRENT || eh.e_version != EV_CURRENT)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_VERSION, "ELF version %u", eh.e_version);
   if (eh.e_type != ET_DYN)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_TYPE, "type %u, not a shared object (ET_DYN)",
                      eh.e_type);
   if (eh.e_machine != EM_X86_64)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_MACHINE,
                      "machine %u, not x86-64 (an arm64-v8a build cannot run natively here)",
                      eh.e_machine);
   info->type = eh.e_type;
   info->machine = eh.e_machine;
   info->osabi = eh.e_ident[7];
   info->entry = eh.e_entry;

   if (eh.e_ehsize < sizeof(elf64_ehdr) || eh.e_phentsize != sizeof(elf64_phdr) ||
       eh.e_phnum == 0 || eh.e_phnum > AKENO_ELF_MAX_PHNUM)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_PHDR, "%u program headers of %u bytes",
                      eh.e_phnum, eh.e_phentsize);
   if (!elf_range_ok(eh.e_phoff, (uint64_t)eh.e_phnum * sizeof(elf64_phdr), size))
      return elf_fail(err, err_size, AKENO_ELF_E_TRUNCATED, "the program headers lie past the file");

   bool have_dynamic = false;
   uint64_t dyn_filesz = 0;
   uint64_t previous_end = 0;
   for (uint16_t i = 0; i < eh.e_phnum; i++) {
      elf64_phdr ph;
      memcpy(&ph, bytes + eh.e_phoff + (uint64_t)i * sizeof(ph), sizeof(ph));
      uint64_t end;
      switch (ph.p_type) {
      case PT_LOAD:
         if (info->load_count == AKENO_ELF_MAX_LOAD)
            return elf_fail(err, err_size, AKENO_ELF_E_BAD_SEGMENT, "more than %d PT_LOAD segments",
                            AKENO_ELF_MAX_LOAD);
         if (ph.p_filesz > ph.p_memsz)
            return elf_fail(err, err_size, AKENO_ELF_E_BAD_SEGMENT,
                            "PT_LOAD %u has p_filesz > p_memsz", info->load_count);
         if (!elf_range_ok(ph.p_offset, ph.p_filesz, size))
            return elf_fail(err, err_size, AKENO_ELF_E_TRUNCATED,
                            "PT_LOAD %u's file bytes lie past the end of the file", info->load_count);
         if (!elf_add(ph.p_vaddr, ph.p_memsz, &end))
            return elf_fail(err, err_size, AKENO_ELF_E_BAD_SEGMENT, "PT_LOAD %u wraps",
                            info->load_count);
         if (ph.p_align > 1) {
            if (!elf_is_pow2(ph.p_align))
               return elf_fail(err, err_size, AKENO_ELF_E_BAD_SEGMENT,
                               "PT_LOAD %u's alignment 0x%llx is not a power of two",
                               info->load_count, (unsigned long long)ph.p_align);
            if ((ph.p_vaddr - ph.p_offset) % ph.p_align != 0)
               return elf_fail(err, err_size, AKENO_ELF_E_BAD_SEGMENT,
                               "PT_LOAD %u's p_offset and p_vaddr are not congruent",
                               info->load_count);
         }
         /* The gABI orders PT_LOADs by address; overlapping ones would let one
          * segment's bytes overwrite another's after it was copied. */
         if (info->load_count > 0 && ph.p_vaddr < previous_end)
            return elf_fail(err, err_size, AKENO_ELF_E_BAD_SEGMENT,
                            "PT_LOAD %u overlaps or precedes the one before it", info->load_count);
         previous_end = end;
         info->load[info->load_count] = (akeno_elf_segment){
            .vaddr = ph.p_vaddr,
            .memsz = ph.p_memsz,
            .offset = ph.p_offset,
            .filesz = ph.p_filesz,
            .align = ph.p_align,
            .flags = ph.p_flags & (AKENO_PF_R | AKENO_PF_W | AKENO_PF_X),
         };
         if (info->load_count == 0 || ph.p_vaddr < info->min_vaddr)
            info->min_vaddr = ph.p_vaddr;
         if (end > info->max_vaddr)
            info->max_vaddr = end;
         if (ph.p_align > info->max_align)
            info->max_align = ph.p_align;
         info->load_flags_union |= ph.p_flags & (AKENO_PF_R | AKENO_PF_W | AKENO_PF_X);
         info->load_count++;
         break;
      case PT_DYNAMIC:
         if (have_dynamic)
            return elf_fail(err, err_size, AKENO_ELF_E_BAD_DYNAMIC, "two PT_DYNAMIC headers");
         have_dynamic = true;
         info->dyn_vaddr = ph.p_vaddr;
         dyn_filesz = ph.p_filesz;
         break;
      case PT_INTERP: info->has_interp = true; break;
      case PT_TLS:
         info->has_tls = true;
         info->tls_memsz = ph.p_memsz;
         break;
      case PT_GNU_RELRO:
         info->has_relro = ph.p_memsz != 0;
         info->relro_vaddr = ph.p_vaddr;
         info->relro_memsz = ph.p_memsz;
         break;
      case PT_GNU_STACK: info->stack_executable = (ph.p_flags & AKENO_PF_X) != 0; break;
      case PT_NOTE:
         if (elf_range_ok(ph.p_offset, ph.p_filesz, size))
            parse_notes(info, bytes, ph.p_offset, ph.p_filesz);
         break;
      default: break;
      }
   }
   if (info->load_count == 0)
      return elf_fail(err, err_size, AKENO_ELF_E_NO_LOAD, "no PT_LOAD segment");
   if (info->max_vaddr - info->min_vaddr > (UINT64_C(1) << 32))
      return elf_fail(err, err_size, AKENO_ELF_E_TOO_LARGE, "the image spans %llu bytes",
                      (unsigned long long)(info->max_vaddr - info->min_vaddr));
   if (info->has_relro && !vaddr_in_memory(info, info->relro_vaddr, info->relro_memsz))
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_SEGMENT, "PT_GNU_RELRO lies outside the image");

   if (!have_dynamic)
      return AKENO_ELF_OK;
   info->has_dynamic = true;
   if (dyn_filesz < sizeof(elf64_dyn) || dyn_filesz % sizeof(elf64_dyn) != 0)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_DYNAMIC, "PT_DYNAMIC is %llu bytes",
                      (unsigned long long)dyn_filesz);
   info->dyn_count = dyn_filesz / sizeof(elf64_dyn);
   if (info->dyn_count > AKENO_ELF_MAX_DYN_ENTRIES)
      return elf_fail(err, err_size, AKENO_ELF_E_BAD_DYNAMIC, "%llu dynamic entries",
                      (unsigned long long)info->dyn_count);
   return parse_dynamic(info, bytes, size, err, err_size);
}

long
akeno_elf_for_each_import(const akeno_elf_info *info, const void *image, size_t size,
                          bool (*fn)(const akeno_elf_import *import, void *ctx), void *ctx)
{
   if (info == NULL || image == NULL || fn == NULL || !info->has_dynamic)
      return -1;
   const struct file_view view = {info, image, size};
   long visited = 0;
   for (uint64_t index = 1; index < info->symbol_count; index++) {
      elf64_sym sym;
      if (!file_read(&view, info->symtab_vaddr + index * sizeof(sym), &sym, sizeof(sym)))
         return -1;
      if (sym.st_shndx != SHN_UNDEF || sym.st_name == 0)
         continue;
      const char *name = file_string(&view, sym.st_name);
      if (name == NULL)
         return -1;
      const akeno_elf_import import = {
         .name = name,
         .version = elf_symbol_version(info, file_read, &view, file_string, index),
         .bind = ELF64_ST_BIND(sym.st_info),
         .type = ELF64_ST_TYPE(sym.st_info),
      };
      visited++;
      if (!fn(&import, ctx))
         break;
   }
   return visited;
}
