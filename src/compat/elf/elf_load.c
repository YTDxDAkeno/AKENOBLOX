/*
 * AkenoBlox PS5 - akeno_elf: copying, relocating and protecting an image.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The order of work is the console's, not Linux's. On the console execute
 * permission is refused at map time and granted afterwards by
 * sceKernelMprotect (PS5_PayloadSDK platform/docs/PROBE.md, quoted in
 * ps5platform/exec.h), and a private file mapping over reserved memory left
 * the pages unusable (prospero-win docs/WINE_PS5_BUILD.md, "Direct memory").
 * So nothing is mapped from the file: one read-write region is reserved, every
 * PT_LOAD is copied into it, relocations are applied while it is all
 * writable, and only then are pages given their segments' permissions.
 *
 * Relocations are read from the caller's image, never from the copy, so a
 * relocation cannot rewrite the table it is being read from.
 */
#include "elf_internal.h"

#include <stdlib.h>

struct akeno_elf_module {
   akeno_elf_info info;
   akeno_mem_ops mem;
   akeno_mem_region region;
   uint64_t span_start; /* page-aligned lowest vaddr */
   size_t span;
   size_t page_size;
   uintptr_t bias;      /* region.base - span_start */
   size_t page_count;
   uint8_t *page_want;  /* protection the segments ask for, per page */
   uint8_t *page_have;  /* protection the platform applied, per page */
   bool constructed;
   int argc;
   char **argv;
   char **envp;
};

struct deferred {
   elf64_rela rela;
   uint64_t ifunc_vaddr; /* the resolver of a self-defined STT_GNU_IFUNC, or 0 */
};

enum sym_state { SYM_UNSEEN = 0, SYM_RESOLVED, SYM_MISSING, SYM_SELF_IFUNC };

struct load_ctx {
   akeno_elf_module *m;
   const uint8_t *image;
   size_t size;
   const akeno_elf_load_options *opt;
   akeno_elf_load_report *report;
   uint64_t *sym_value;
   uint8_t *sym_state;
   struct deferred *deferred;
   size_t deferred_count, deferred_capacity;
   uint32_t missing; /* imports nothing answered, counted once each */
   akeno_elf_status status; /* the first hard failure */
};

/* ------------------------------------------------------------- helpers */

static uint64_t
floor_to(uint64_t value, uint64_t align)
{
   return value & ~(align - 1);
}

static bool
ceil_to(uint64_t value, uint64_t align, uint64_t *out)
{
   uint64_t sum;
   if (!elf_add(value, align - 1, &sum))
      return false;
   *out = floor_to(sum, align);
   return true;
}

static uint8_t *
mod_ptr(const akeno_elf_module *m, uint64_t vaddr, uint64_t length)
{
   if (vaddr < m->span_start)
      return NULL;
   const uint64_t into = vaddr - m->span_start;
   if (into > m->span || length > m->span - into)
      return NULL;
   return (uint8_t *)m->region.base + into;
}

static bool
image_read(const void *ctx, uint64_t vaddr, void *out, size_t length)
{
   const struct load_ctx *lc = ctx;
   uint64_t offset;
   if (!elf_vaddr_to_offset(&lc->m->info, vaddr, length, &offset) ||
       !elf_range_ok(offset, length, lc->size))
      return false;
   memcpy(out, lc->image + offset, length);
   return true;
}

static const char *
image_string(const void *ctx, uint64_t offset)
{
   const struct load_ctx *lc = ctx;
   return akeno_elf_dynstr(&lc->m->info, lc->image, lc->size, offset);
}

static akeno_elf_status
fail(struct load_ctx *lc, akeno_elf_status status, const char *format, ...)
   __attribute__((format(printf, 3, 4)));

static akeno_elf_status
fail(struct load_ctx *lc, akeno_elf_status status, const char *format, ...)
{
   if (lc->status == AKENO_ELF_OK) {
      lc->status = status;
      if (lc->report != NULL) {
         va_list args;
         va_start(args, format);
         vsnprintf(lc->report->message, sizeof(lc->report->message), format, args);
         va_end(args);
      }
   }
   return status;
}

static void
note_missing(struct load_ctx *lc, const char *name)
{
   lc->missing++;
   akeno_elf_load_report *r = lc->report;
   if (r == NULL)
      return;
   if (r->missing_count < AKENO_ELF_MISSING_KEPT) {
      snprintf(r->missing[r->missing_count], AKENO_ELF_NAME_BYTES, "%s", name);
   }
   r->missing_count++;
}

/* ------------------------------------------------------------- symbols */

/* The value a relocation's symbol index stands for. Returns OK and sets
 * *value, or sets *deferred_ifunc for a self-defined IFUNC whose value only
 * exists once its resolver has run, or reports MISSING without failing so
 * every missing import is named before the load gives up. */
static akeno_elf_status
symbol_value(struct load_ctx *lc, uint32_t index, uint64_t *value, uint64_t *ifunc_vaddr,
             bool *missing)
{
   *value = 0;
   *ifunc_vaddr = 0;
   *missing = false;
   if (index == 0)
      return AKENO_ELF_OK;
   const akeno_elf_info *info = &lc->m->info;
   if (index >= info->symbol_count)
      return fail(lc, AKENO_ELF_E_BAD_RELOC, "relocation names symbol %u of %llu", index,
                  (unsigned long long)info->symbol_count);
   if (lc->sym_state != NULL) {
      switch (lc->sym_state[index]) {
      case SYM_RESOLVED: *value = lc->sym_value[index]; return AKENO_ELF_OK;
      case SYM_MISSING: *missing = true; return AKENO_ELF_OK;
      case SYM_SELF_IFUNC: *ifunc_vaddr = lc->sym_value[index]; return AKENO_ELF_OK;
      default: break;
      }
   }
   elf64_sym sym;
   if (!image_read(lc, info->symtab_vaddr + (uint64_t)index * sizeof(sym), &sym, sizeof(sym)))
      return fail(lc, AKENO_ELF_E_BAD_SYMTAB, "symbol %u lies outside the file", index);
   const uint8_t type = ELF64_ST_TYPE(sym.st_info), bind = ELF64_ST_BIND(sym.st_info);
   if (type == STT_TLS)
      return fail(lc, AKENO_ELF_E_TLS_UNSUPPORTED,
                  "symbol %u is thread-local; the console's TLS is emulated (-femulated-tls)",
                  index);
   uint8_t state = SYM_RESOLVED;
   uint64_t result = 0;
   if (sym.st_shndx != SHN_UNDEF) {
      /* Defined here: the object's own definition, as bionic's local group
       * finds it first for a library loaded with no preloads. */
      const bool absolute = sym.st_shndx == 0xfff1; /* SHN_ABS */
      result = absolute ? sym.st_value : lc->m->bias + sym.st_value;
      if (!absolute && mod_ptr(lc->m, sym.st_value, 1) == NULL && sym.st_size != 0)
         return fail(lc, AKENO_ELF_E_BAD_SYMTAB, "symbol %u's value lies outside the image", index);
      if (type == STT_GNU_IFUNC) {
         state = SYM_SELF_IFUNC;
         result = sym.st_value;
         *ifunc_vaddr = result;
      } else {
         *value = result;
      }
   } else {
      const char *name = image_string(lc, sym.st_name);
      if (name == NULL || name[0] == '\0')
         return fail(lc, AKENO_ELF_E_BAD_STRTAB, "undefined symbol %u has no name", index);
      const char *version = elf_symbol_version(info, image_read, lc, image_string, index);
      void *address = lc->opt->resolve != NULL ? lc->opt->resolve(lc->opt->resolve_ctx, name, version)
                                              : NULL;
      if (address == NULL && bind != STB_WEAK) {
         state = SYM_MISSING;
         *missing = true;
         note_missing(lc, name);
      } else {
         result = (uint64_t)(uintptr_t)address;
         *value = result;
      }
   }
   if (lc->sym_state != NULL) {
      lc->sym_state[index] = state;
      lc->sym_value[index] = result;
   }
   return AKENO_ELF_OK;
}

/* --------------------------------------------------------- relocations */

static akeno_elf_status
write_word(struct load_ctx *lc, uint64_t vaddr, uint64_t value, unsigned width)
{
   uint8_t *where = mod_ptr(lc->m, vaddr, width);
   if (where == NULL)
      return fail(lc, AKENO_ELF_E_BAD_RELOC, "relocation target 0x%llx lies outside the image",
                  (unsigned long long)vaddr);
   if (width == 8) {
      memcpy(where, &value, 8);
   } else {
      const uint32_t narrow = (uint32_t)value;
      memcpy(where, &narrow, 4);
   }
   return AKENO_ELF_OK;
}

static akeno_elf_status
defer(struct load_ctx *lc, const elf64_rela *r, uint64_t ifunc_vaddr)
{
   if (!lc->opt->allow_ifunc)
      return fail(lc, AKENO_ELF_E_IFUNC_REFUSED,
                  "relocation at 0x%llx needs an IFUNC resolver to run during loading",
                  (unsigned long long)r->r_offset);
   if (lc->deferred_count == lc->deferred_capacity) {
      const size_t capacity = lc->deferred_capacity ? lc->deferred_capacity * 2 : 16;
      struct deferred *grown = realloc(lc->deferred, capacity * sizeof(*grown));
      if (grown == NULL)
         return fail(lc, AKENO_ELF_E_NO_MEMORY, "no memory for deferred relocations");
      lc->deferred = grown;
      lc->deferred_capacity = capacity;
   }
   lc->deferred[lc->deferred_count++] = (struct deferred){*r, ifunc_vaddr};
   return AKENO_ELF_OK;
}

/* S + A - P must fit 32 signed bits. */
static bool
fits_s32(int64_t value)
{
   return value >= INT32_MIN && value <= INT32_MAX;
}

static akeno_elf_status
apply_rela(struct load_ctx *lc, const elf64_rela *r)
{
   const uint32_t type = ELF64_R_TYPE(r->r_info);
   const uint32_t index = ELF64_R_SYM(r->r_info);
   const uint64_t bias = lc->m->bias;
   const uint64_t place = bias + r->r_offset;
   uint64_t s = 0, ifunc = 0;
   bool missing = false;

   switch (type) {
   case R_X86_64_NONE: return AKENO_ELF_OK;
   case R_X86_64_RELATIVE:
      if (index != 0)
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "R_X86_64_RELATIVE names symbol %u", index);
      if (lc->report)
         lc->report->relative_applied++;
      return write_word(lc, r->r_offset, bias + (uint64_t)r->r_addend, 8);
   case R_X86_64_IRELATIVE:
      if (index != 0)
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "R_X86_64_IRELATIVE names symbol %u", index);
      if (mod_ptr(lc->m, r->r_offset, 8) == NULL)
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "IRELATIVE target lies outside the image");
      return defer(lc, r, 0);
   case R_X86_64_COPY:
      return fail(lc, AKENO_ELF_E_UNSUPPORTED_RELOC,
                  "R_X86_64_COPY belongs to executables, not shared objects");
   case R_X86_64_DTPMOD64:
   case R_X86_64_DTPOFF64:
   case R_X86_64_TPOFF64:
      return fail(lc, AKENO_ELF_E_TLS_UNSUPPORTED,
                  "TLS relocation %u at 0x%llx; the console's TLS is emulated", type,
                  (unsigned long long)r->r_offset);
   case R_X86_64_64:
   case R_X86_64_GLOB_DAT:
   case R_X86_64_JUMP_SLOT:
   case R_X86_64_PC32:
   case R_X86_64_PC64:
   case R_X86_64_32:
   case R_X86_64_32S:
      break;
   default:
      return fail(lc, AKENO_ELF_E_UNSUPPORTED_RELOC, "relocation type %u at 0x%llx", type,
                  (unsigned long long)r->r_offset);
   }

   akeno_elf_status st = symbol_value(lc, index, &s, &ifunc, &missing);
   if (st != AKENO_ELF_OK)
      return st;
   if (missing)
      return AKENO_ELF_OK; /* named in the report; the load fails once all are seen */
   if (ifunc != 0) {
      if (mod_ptr(lc->m, r->r_offset, 8) == NULL)
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "IFUNC relocation target lies outside the image");
      return defer(lc, r, ifunc);
   }
   if (lc->report)
      lc->report->symbolic_applied++;
   const int64_t a = r->r_addend;
   switch (type) {
   case R_X86_64_64:
   case R_X86_64_GLOB_DAT:
   case R_X86_64_JUMP_SLOT: return write_word(lc, r->r_offset, s + (uint64_t)a, 8);
   case R_X86_64_PC64: return write_word(lc, r->r_offset, s + (uint64_t)a - place, 8);
   case R_X86_64_PC32: {
      const int64_t v = (int64_t)(s + (uint64_t)a - place);
      if (!fits_s32(v))
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "R_X86_64_PC32 at 0x%llx overflows",
                     (unsigned long long)r->r_offset);
      return write_word(lc, r->r_offset, (uint64_t)v, 4);
   }
   case R_X86_64_32: {
      const uint64_t v = s + (uint64_t)a;
      if (v > UINT32_MAX)
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "R_X86_64_32 at 0x%llx overflows",
                     (unsigned long long)r->r_offset);
      return write_word(lc, r->r_offset, v, 4);
   }
   case R_X86_64_32S: {
      const int64_t v = (int64_t)(s + (uint64_t)a);
      if (!fits_s32(v))
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "R_X86_64_32S at 0x%llx overflows",
                     (unsigned long long)r->r_offset);
      return write_word(lc, r->r_offset, (uint64_t)v, 4);
   }
   default: return AKENO_ELF_OK;
   }
}

static akeno_elf_status
apply_rela_table(struct load_ctx *lc, uint64_t vaddr, uint64_t bytes)
{
   for (uint64_t at = 0; at < bytes; at += sizeof(elf64_rela)) {
      elf64_rela r;
      if (!image_read(lc, vaddr + at, &r, sizeof(r)))
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "relocation table entry lies outside the file");
      if (apply_rela(lc, &r) != AKENO_ELF_OK)
         return lc->status;
      if (lc->report)
         lc->report->relocations_applied++;
   }
   return AKENO_ELF_OK;
}

/* RELR: an even word is an address to relocate, an odd word a bitmap of the
 * 63 words after the last address (the gABI's SHT_RELR, which is Android's
 * SHT_ANDROID_RELR word for word). */
static akeno_elf_status
apply_relr(struct load_ctx *lc, uint64_t vaddr, uint64_t bytes)
{
   const uint64_t bias = lc->m->bias;
   uint64_t base = 0;
   bool have_base = false;
   for (uint64_t at = 0; at < bytes; at += 8) {
      uint64_t entry;
      if (!image_read(lc, vaddr + at, &entry, sizeof(entry)))
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "DT_RELR entry lies outside the file");
      if ((entry & 1u) == 0) {
         uint8_t *where = mod_ptr(lc->m, entry, 8);
         if (where == NULL)
            return fail(lc, AKENO_ELF_E_BAD_RELOC, "DT_RELR address 0x%llx lies outside the image",
                        (unsigned long long)entry);
         uint64_t value;
         memcpy(&value, where, 8);
         value += bias;
         memcpy(where, &value, 8);
         base = entry + 8;
         have_base = true;
         if (lc->report) {
            lc->report->relative_applied++;
            lc->report->relocations_applied++;
         }
         continue;
      }
      if (!have_base)
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "DT_RELR starts with a bitmap");
      uint64_t bits = entry >> 1;
      for (uint64_t slot = 0; bits != 0; slot++, bits >>= 1) {
         if ((bits & 1u) == 0)
            continue;
         uint8_t *where = mod_ptr(lc->m, base + slot * 8, 8);
         if (where == NULL)
            return fail(lc, AKENO_ELF_E_BAD_RELOC, "DT_RELR bitmap reaches outside the image");
         uint64_t value;
         memcpy(&value, where, 8);
         value += bias;
         memcpy(where, &value, 8);
         if (lc->report) {
            lc->report->relative_applied++;
            lc->report->relocations_applied++;
         }
      }
      base += 63 * 8;
   }
   return AKENO_ELF_OK;
}

/* APS2, bionic's packed relocations: "APS2", then a SLEB128 stream of a count,
 * a starting offset and groups sharing an offset delta, an r_info or an
 * addend (bionic linker/linker_reloc_iterators.h describes the encoding). */
struct sleb {
   const uint8_t *at, *end;
   bool ok;
};

static int64_t
sleb_next(struct sleb *s)
{
   int64_t value = 0;
   unsigned shift = 0;
   uint8_t byte;
   do {
      if (s->at == s->end || shift >= 64) {
         s->ok = false;
         return 0;
      }
      byte = *s->at++;
      value |= (int64_t)((uint64_t)(byte & 0x7f) << shift);
      shift += 7;
   } while (byte & 0x80);
   if (shift < 64 && (byte & 0x40))
      value |= (int64_t)(~UINT64_C(0) << shift);
   return value;
}

#define APS2_GROUPED_BY_INFO 1u
#define APS2_GROUPED_BY_OFFSET_DELTA 2u
#define APS2_GROUPED_BY_ADDEND 4u
#define APS2_GROUP_HAS_ADDEND 8u

static akeno_elf_status
apply_aps2(struct load_ctx *lc, uint64_t vaddr, uint64_t bytes)
{
   uint64_t offset;
   if (!elf_vaddr_to_offset(&lc->m->info, vaddr, bytes, &offset) ||
       !elf_range_ok(offset, bytes, lc->size))
      return fail(lc, AKENO_ELF_E_BAD_RELOC, "DT_ANDROID_RELA lies outside the file");
   if (bytes < 4 || memcmp(lc->image + offset, "APS2", 4) != 0)
      return fail(lc, AKENO_ELF_E_BAD_RELOC, "DT_ANDROID_RELA does not start with APS2");
   struct sleb s = {lc->image + offset + 4, lc->image + offset + bytes, true};
   const int64_t count = sleb_next(&s);
   elf64_rela r = {.r_offset = (uint64_t)sleb_next(&s), .r_info = 0, .r_addend = 0};
   if (!s.ok || count < 0 || count > (INT64_C(1) << 28))
      return fail(lc, AKENO_ELF_E_BAD_RELOC, "APS2 header is malformed");
   for (int64_t done = 0; done < count;) {
      const int64_t group_size = sleb_next(&s);
      const uint64_t flags = (uint64_t)sleb_next(&s);
      if (!s.ok || group_size <= 0 || group_size > count - done)
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "APS2 group of %lld at relocation %lld",
                     (long long)group_size, (long long)done);
      uint64_t delta = 0;
      if (flags & APS2_GROUPED_BY_OFFSET_DELTA)
         delta = (uint64_t)sleb_next(&s);
      if (flags & APS2_GROUPED_BY_INFO)
         r.r_info = (uint64_t)sleb_next(&s);
      const uint64_t addend_mode = flags & (APS2_GROUP_HAS_ADDEND | APS2_GROUPED_BY_ADDEND);
      if (addend_mode == (APS2_GROUP_HAS_ADDEND | APS2_GROUPED_BY_ADDEND))
         r.r_addend += sleb_next(&s);
      else if (addend_mode != APS2_GROUP_HAS_ADDEND)
         r.r_addend = 0;
      for (int64_t i = 0; i < group_size; i++) {
         r.r_offset += (flags & APS2_GROUPED_BY_OFFSET_DELTA) ? delta : (uint64_t)sleb_next(&s);
         if ((flags & APS2_GROUPED_BY_INFO) == 0)
            r.r_info = (uint64_t)sleb_next(&s);
         if (addend_mode == APS2_GROUP_HAS_ADDEND)
            r.r_addend += sleb_next(&s);
         if (!s.ok)
            return fail(lc, AKENO_ELF_E_BAD_RELOC, "APS2 stream ends inside relocation %lld",
                        (long long)(done + i));
         if (apply_rela(lc, &r) != AKENO_ELF_OK)
            return lc->status;
         if (lc->report)
            lc->report->relocations_applied++;
      }
      done += group_size;
   }
   return AKENO_ELF_OK;
}

/* ---------------------------------------------------------- protection */

static unsigned
pf_to_prot(uint32_t flags)
{
   unsigned prot = 0;
   if (flags & AKENO_PF_R)
      prot |= AKENO_PROT_READ;
   if (flags & AKENO_PF_W)
      prot |= AKENO_PROT_WRITE;
   if (flags & AKENO_PF_X)
      prot |= AKENO_PROT_EXEC;
   return prot;
}

/* Applies `want` to every page in [first, last) in runs of equal protection. */
static akeno_elf_status
protect_pages(struct load_ctx *lc, size_t first, size_t last, const uint8_t *want)
{
   akeno_elf_module *m = lc->m;
   size_t run = first;
   while (run < last) {
      size_t end = run + 1;
      while (end < last && want[end] == want[run])
         end++;
      unsigned applied = 0;
      const int rc = m->mem.protect(m->mem.ctx, &m->region, run * m->page_size,
                                    (end - run) * m->page_size, want[run], &applied);
      if (rc != 0)
         return fail(lc, AKENO_ELF_E_PROTECT_FAILED,
                     "the platform refused protection %u for pages %zu-%zu (code %d)", want[run],
                     run, end - 1, rc);
      for (size_t p = run; p < end; p++)
         m->page_have[p] = (uint8_t)applied;
      if (lc->report)
         lc->report->applied_union |= applied;
      run = end;
   }
   return AKENO_ELF_OK;
}

static bool
page_has(const akeno_elf_module *m, uint64_t vaddr, unsigned prot)
{
   const uint8_t *p = mod_ptr(m, vaddr, 1);
   if (p == NULL)
      return false;
   const size_t page = (size_t)(p - (const uint8_t *)m->region.base) / m->page_size;
   return (m->page_have[page] & prot) == prot;
}

/* Like mod_ptr, for reads after protection: NULL unless every page the range
 * touches was left readable. A malformed image can point a table at a gap
 * between segments (no access) or at a segment without PF_R; reading it would
 * fault instead of refusing (found by tests/fuzz/fuzz_elf_load.c). */
static const uint8_t *
mod_readable(const akeno_elf_module *m, uint64_t vaddr, uint64_t length)
{
   const uint8_t *p = mod_ptr(m, vaddr, length);
   if (p == NULL || length == 0)
      return p;
   const size_t first = (size_t)(p - (const uint8_t *)m->region.base) / m->page_size;
   const size_t last = (size_t)(p + length - 1 - (const uint8_t *)m->region.base) / m->page_size;
   for (size_t page = first; page <= last; page++)
      if ((m->page_have[page] & AKENO_PROT_READ) == 0)
         return NULL;
   return p;
}

typedef uint64_t (*ifunc_resolver)(void);
typedef void (*init_function)(int, char **, char **);

static akeno_elf_status
run_deferred(struct load_ctx *lc)
{
   akeno_elf_module *m = lc->m;
   for (size_t i = 0; i < lc->deferred_count; i++) {
      const struct deferred *d = &lc->deferred[i];
      const uint32_t type = ELF64_R_TYPE(d->rela.r_info);
      const uint64_t resolver_vaddr =
         d->ifunc_vaddr != 0 ? d->ifunc_vaddr : (uint64_t)d->rela.r_addend;
      if (!page_has(m, resolver_vaddr, AKENO_PROT_EXEC))
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "IFUNC resolver at 0x%llx is not executable code",
                     (unsigned long long)resolver_vaddr);
      if (!page_has(m, d->rela.r_offset, AKENO_PROT_WRITE))
         return fail(lc, AKENO_ELF_E_BAD_RELOC, "IFUNC target 0x%llx is not writable",
                     (unsigned long long)d->rela.r_offset);
      const ifunc_resolver resolver = (ifunc_resolver)(m->bias + resolver_vaddr);
      uint64_t value = resolver();
      if (type != R_X86_64_IRELATIVE)
         value += (uint64_t)d->rela.r_addend;
      if (write_word(lc, d->rela.r_offset, value, 8) != AKENO_ELF_OK)
         return lc->status;
      if (lc->report)
         lc->report->deferred_applied++;
   }
   return AKENO_ELF_OK;
}

/* A constructor or destructor pointer must land on this module's code. */
static bool
callable(const akeno_elf_module *m, uint64_t address)
{
   if (address < m->bias)
      return false;
   return page_has(m, address - m->bias, AKENO_PROT_EXEC);
}

static akeno_elf_status
run_constructors(struct load_ctx *lc)
{
   akeno_elf_module *m = lc->m;
   const akeno_elf_info *info = &m->info;
   if (info->has_init) {
      const uint64_t address = m->bias + info->init_vaddr;
      if (!callable(m, address))
         return fail(lc, AKENO_ELF_E_BAD_DYNAMIC, "DT_INIT is not executable code");
      ((init_function)address)(m->argc, m->argv, m->envp);
   }
   for (uint64_t i = 0; i < info->init_array_count; i++) {
      uint64_t address;
      const uint8_t *slot = mod_readable(m, info->init_array_vaddr + i * 8, 8);
      if (slot == NULL)
         return fail(lc, AKENO_ELF_E_BAD_DYNAMIC, "DT_INIT_ARRAY is not readable");
      memcpy(&address, slot, 8);
      /* bionic skips null and -1 entries. */
      if (address == 0 || address == UINT64_MAX)
         continue;
      if (!callable(m, address))
         return fail(lc, AKENO_ELF_E_BAD_DYNAMIC,
                     "DT_INIT_ARRAY[%llu] = 0x%llx is not this module's code",
                     (unsigned long long)i, (unsigned long long)address);
      ((init_function)address)(m->argc, m->argv, m->envp);
   }
   m->constructed = true;
   return AKENO_ELF_OK;
}

static void
run_destructors(akeno_elf_module *m)
{
   const akeno_elf_info *info = &m->info;
   for (uint64_t i = info->fini_array_count; i-- > 0;) {
      uint64_t address;
      const uint8_t *slot = mod_readable(m, info->fini_array_vaddr + i * 8, 8);
      if (slot == NULL)
         return;
      memcpy(&address, slot, 8);
      if (address == 0 || address == UINT64_MAX || !callable(m, address))
         continue;
      ((void (*)(void))address)();
   }
   if (info->has_fini && callable(m, m->bias + info->fini_vaddr))
      ((void (*)(void))(m->bias + info->fini_vaddr))();
}

/* ---------------------------------------------------------------- load */

static void
free_module(akeno_elf_module *m)
{
   if (m == NULL)
      return;
   if (m->region.base != NULL)
      m->mem.release(m->mem.ctx, &m->region);
   free(m->page_want);
   free(m->page_have);
   free(m);
}

akeno_elf_status
akeno_elf_load(const void *image, size_t size, const akeno_elf_load_options *opt,
               akeno_elf_module **module, akeno_elf_load_report *report)
{
   if (report != NULL)
      memset(report, 0, sizeof(*report));
   if (module != NULL)
      *module = NULL;
   if (image == NULL || opt == NULL || opt->mem == NULL || module == NULL ||
       opt->mem->reserve == NULL || opt->mem->protect == NULL || opt->mem->release == NULL ||
       !elf_is_pow2(opt->mem->page_size) || opt->mem->page_size < 4096) {
      if (report != NULL) {
         report->status = AKENO_ELF_E_INVALID_ARGUMENT;
         snprintf(report->message, sizeof(report->message), "missing image, options or memory");
      }
      return AKENO_ELF_E_INVALID_ARGUMENT;
   }

   akeno_elf_module *m = calloc(1, sizeof(*m));
   struct load_ctx lc = {.m = m, .image = image, .size = size, .opt = opt, .report = report};
   if (m == NULL) {
      fail(&lc, AKENO_ELF_E_NO_MEMORY, "no memory for the module record");
      goto out;
   }
   m->mem = *opt->mem;
   m->page_size = opt->mem->page_size;
   m->argc = opt->argc;
   m->argv = opt->argv;
   m->envp = opt->envp;

   char why[160];
   const akeno_elf_status parsed = akeno_elf_parse(image, size, &m->info, why, sizeof(why));
   if (parsed != AKENO_ELF_OK) {
      fail(&lc, parsed, "%s", why);
      goto out;
   }
   const akeno_elf_info *info = &m->info;
   if (info->has_interp) {
      fail(&lc, AKENO_ELF_E_BAD_TYPE, "a position-independent executable, not a library");
      goto out;
   }
   if (info->relocs.has_rel) {
      fail(&lc, AKENO_ELF_E_UNSUPPORTED_RELOC, "REL relocations; x86-64 objects use RELA");
      goto out;
   }
   if (info->has_tls) {
      fail(&lc, AKENO_ELF_E_TLS_UNSUPPORTED,
           "the object has a PT_TLS segment of %llu bytes; only emulated TLS can run on the "
           "console",
           (unsigned long long)info->tls_memsz);
      goto out;
   }

   /* The span, in the platform's pages. A library aligned for 4 KiB pages
    * shares 16 KiB console pages between segments; those pages get the union
    * of the segments' permissions, counted in wx_pages when that union is
    * writable and executable (PS5_VulkanTemplate memory-and-jit.md: pages are
    * 16 KiB). */
   const uint64_t page = m->page_size;
   const uint64_t max_span = opt->max_span ? opt->max_span : (UINT64_C(2) << 30);
   uint64_t span_end;
   m->span_start = floor_to(info->min_vaddr, page);
   if (!ceil_to(info->max_vaddr, page, &span_end) || span_end - m->span_start > max_span ||
       span_end - m->span_start > SIZE_MAX) {
      fail(&lc, AKENO_ELF_E_TOO_LARGE, "the image spans more than %llu bytes",
           (unsigned long long)max_span);
      goto out;
   }
   m->span = (size_t)(span_end - m->span_start);
   m->page_count = m->span / page;
   m->page_want = calloc(m->page_count, 1);
   m->page_have = calloc(m->page_count, 1);
   if (m->page_want == NULL || m->page_have == NULL) {
      fail(&lc, AKENO_ELF_E_NO_MEMORY, "no memory for %zu page records", m->page_count);
      goto out;
   }
   uint32_t wx_pages = 0;
   for (uint32_t i = 0; i < info->load_count; i++) {
      const akeno_elf_segment *s = &info->load[i];
      if (s->memsz == 0)
         continue;
      const size_t first = (size_t)((floor_to(s->vaddr, page) - m->span_start) / page);
      const size_t last = (size_t)((floor_to(s->vaddr + s->memsz - 1, page) - m->span_start) / page);
      for (size_t p = first; p <= last; p++)
         m->page_want[p] |= (uint8_t)pf_to_prot(s->flags);
   }
   for (size_t p = 0; p < m->page_count; p++)
      if ((m->page_want[p] & (AKENO_PROT_WRITE | AKENO_PROT_EXEC)) ==
          (AKENO_PROT_WRITE | AKENO_PROT_EXEC))
         wx_pages++;
   if (report != NULL)
      report->wx_pages = wx_pages;

   uint64_t align = page;
   if (info->max_align > align)
      align = info->max_align > (UINT64_C(2) << 20) ? (UINT64_C(2) << 20) : info->max_align;
   const int rc = m->mem.reserve(m->mem.ctx, m->span, (size_t)align,
                                 wx_pages ? AKENO_MEM_NEED_WX : 0u, &m->region);
   if (rc != 0 || m->region.base == NULL) {
      m->region.base = NULL;
      fail(&lc, AKENO_ELF_E_NO_MEMORY, "the platform could not provide %zu bytes (code %d)",
           m->span, rc);
      goto out;
   }
   if (m->region.size < m->span || ((uintptr_t)m->region.base & (align - 1)) != 0) {
      m->mem.release(m->mem.ctx, &m->region);
      m->region.base = NULL;
      fail(&lc, AKENO_ELF_E_NO_MEMORY, "the platform's region is too small or misaligned");
      goto out;
   }
   m->bias = (uintptr_t)m->region.base - (uintptr_t)m->span_start;

   /* Copy every PT_LOAD and zero what the file does not supply. */
   for (uint32_t i = 0; i < info->load_count; i++) {
      const akeno_elf_segment *s = &info->load[i];
      uint8_t *dst = mod_ptr(m, s->vaddr, s->memsz);
      if (dst == NULL) {
         fail(&lc, AKENO_ELF_E_BAD_SEGMENT, "PT_LOAD %u lies outside the span", i);
         goto out;
      }
      memcpy(dst, (const uint8_t *)image + s->offset, (size_t)s->filesz);
      memset(dst + s->filesz, 0, (size_t)(s->memsz - s->filesz));
   }

   if (info->has_dynamic) {
      if (info->symbol_count > 0 && info->symbol_count <= (UINT64_C(1) << 22)) {
         lc.sym_value = calloc((size_t)info->symbol_count, sizeof(*lc.sym_value));
         lc.sym_state = calloc((size_t)info->symbol_count, 1);
         if (lc.sym_value == NULL || lc.sym_state == NULL) {
            free(lc.sym_value);
            free(lc.sym_state);
            lc.sym_value = NULL;
            lc.sym_state = NULL;
         }
      }
      /* bionic's order: packed, RELR, RELA, then the PLT. */
      if (info->relocs.android_rela_bytes &&
          apply_aps2(&lc, info->android_rela_vaddr, info->relocs.android_rela_bytes) != AKENO_ELF_OK)
         goto out;
      if (info->relocs.relr_bytes &&
          apply_relr(&lc, info->relr_vaddr, info->relocs.relr_bytes) != AKENO_ELF_OK)
         goto out;
      if (info->relocs.rela_bytes &&
          apply_rela_table(&lc, info->rela_vaddr, info->relocs.rela_bytes) != AKENO_ELF_OK)
         goto out;
      if (info->relocs.plt_bytes &&
          apply_rela_table(&lc, info->jmprel_vaddr, info->relocs.plt_bytes) != AKENO_ELF_OK)
         goto out;
      if (lc.missing > 0) {
         char list[128] = "";
         size_t used = 0;
         for (uint32_t i = 0; report != NULL && i < report->missing_count &&
                              i < AKENO_ELF_MISSING_KEPT && used < 100;
              i++)
            used += (size_t)snprintf(list + used, sizeof(list) - used, "%s%s", i ? ", " : "",
                                     report->missing[i]);
         fail(&lc, AKENO_ELF_E_UNDEFINED_SYMBOL, "%u undefined: %s", lc.missing, list);
         goto out;
      }
   }

   if (protect_pages(&lc, 0, m->page_count, m->page_want) != AKENO_ELF_OK)
      goto out;
   if (run_deferred(&lc) != AKENO_ELF_OK)
      goto out;
   if (info->has_relro) {
      /* Inward: a RELRO end that is not page-aligned for these pages (a 4 KiB
       * library on 16 KiB pages) must not take writes away from .data. */
      uint64_t first_vaddr;
      if (ceil_to(info->relro_vaddr, page, &first_vaddr)) {
         const uint64_t last_vaddr = floor_to(info->relro_vaddr + info->relro_memsz, page);
         if (last_vaddr > first_vaddr) {
            const size_t first = (size_t)((first_vaddr - m->span_start) / page);
            const size_t last = (size_t)((last_vaddr - m->span_start) / page);
            uint8_t *relro = malloc(m->page_count);
            if (relro == NULL) {
               fail(&lc, AKENO_ELF_E_NO_MEMORY, "no memory for the RELRO pass");
               goto out;
            }
            memcpy(relro, m->page_want, m->page_count);
            for (size_t p = first; p < last; p++)
               relro[p] = (uint8_t)(relro[p] & ~AKENO_PROT_WRITE);
            const akeno_elf_status st = protect_pages(&lc, first, last, relro);
            free(relro);
            if (st != AKENO_ELF_OK)
               goto out;
         }
      }
   }
   if (opt->run_constructors && run_constructors(&lc) != AKENO_ELF_OK)
      goto out;

out:
   free(lc.sym_value);
   free(lc.sym_state);
   free(lc.deferred);
   if (report != NULL)
      report->status = lc.status;
   if (lc.status != AKENO_ELF_OK) {
      free_module(m);
      return lc.status;
   }
   *module = m;
   return AKENO_ELF_OK;
}

/* -------------------------------------------------------------- lookup */

static uint32_t
gnu_hash(const char *name)
{
   uint32_t h = 5381;
   for (const unsigned char *c = (const unsigned char *)name; *c; c++)
      h = h * 33u + *c;
   return h;
}

static uint32_t
sysv_hash(const char *name)
{
   uint32_t h = 0;
   for (const unsigned char *c = (const unsigned char *)name; *c; c++) {
      h = (h << 4) + *c;
      const uint32_t g = h & 0xf0000000u;
      if (g)
         h ^= g >> 24;
      h &= ~g;
   }
   return h;
}

static bool
read_mem(const akeno_elf_module *m, uint64_t vaddr, void *out, size_t length)
{
   const uint8_t *p = mod_readable(m, vaddr, length);
   if (p == NULL)
      return false;
   memcpy(out, p, length);
   return true;
}

/* Symbol index matches name and is an exported definition. */
static void *
match(const akeno_elf_module *m, uint64_t index, const char *name)
{
   const akeno_elf_info *info = &m->info;
   if (index == 0 || index >= info->symbol_count)
      return NULL;
   elf64_sym sym;
   if (!read_mem(m, info->symtab_vaddr + index * sizeof(sym), &sym, sizeof(sym)))
      return NULL;
   const uint8_t bind = ELF64_ST_BIND(sym.st_info), type = ELF64_ST_TYPE(sym.st_info);
   const uint8_t visibility = ELF64_ST_VISIBILITY(sym.st_other);
   if (sym.st_shndx == SHN_UNDEF || type == STT_TLS ||
       (bind != STB_GLOBAL && bind != STB_WEAK && bind != STB_GNU_UNIQUE) ||
       (visibility != STV_DEFAULT && visibility != STV_PROTECTED) ||
       sym.st_name >= info->strtab_size)
      return NULL;
   const size_t room = (size_t)(info->strtab_size - sym.st_name);
   const size_t length = strlen(name);
   if (length >= room)
      return NULL;
   const char *candidate =
      (const char *)mod_readable(m, info->strtab_vaddr + sym.st_name, length + 1);
   if (candidate == NULL || memcmp(candidate, name, length + 1) != 0)
      return NULL;
   if (type == STT_GNU_IFUNC)
      return NULL; /* its address is its resolver's answer, not the symbol's value */
   if (sym.st_shndx == 0xfff1)
      return (void *)(uintptr_t)sym.st_value;
   if (mod_ptr(m, sym.st_value, 1) == NULL)
      return NULL;
   return (void *)(m->bias + sym.st_value);
}

void *
akeno_elf_symbol(const akeno_elf_module *m, const char *name)
{
   if (m == NULL || name == NULL || !m->info.has_dynamic)
      return NULL;
   const akeno_elf_info *info = &m->info;
   if (info->gnu_hash_vaddr != 0) {
      uint32_t header[4];
      if (!read_mem(m, info->gnu_hash_vaddr, header, sizeof(header)))
         return NULL;
      const uint32_t nbuckets = header[0], symoffset = header[1];
      const uint32_t bloom_size = header[2], shift = header[3];
      if (nbuckets == 0 || bloom_size == 0)
         return NULL;
      const uint32_t h = gnu_hash(name);
      uint64_t word;
      if (!read_mem(m, info->gnu_hash_vaddr + 16 + (uint64_t)((h / 64) % bloom_size) * 8, &word, 8))
         return NULL;
      const uint64_t mask = (UINT64_C(1) << (h % 64)) | (UINT64_C(1) << ((h >> (shift & 31)) % 64));
      if ((word & mask) != mask)
         return NULL;
      const uint64_t buckets = info->gnu_hash_vaddr + 16 + (uint64_t)bloom_size * 8;
      uint32_t index;
      if (!read_mem(m, buckets + (uint64_t)(h % nbuckets) * 4, &index, 4) || index < symoffset)
         return NULL;
      const uint64_t chain = buckets + (uint64_t)nbuckets * 4;
      for (uint64_t i = index; i < info->symbol_count; i++) {
         uint32_t h2;
         if (!read_mem(m, chain + (i - symoffset) * 4, &h2, 4))
            return NULL;
         if ((h | 1u) == (h2 | 1u)) {
            void *found = match(m, i, name);
            if (found != NULL)
               return found;
         }
         if (h2 & 1u)
            break;
      }
      return NULL;
   }
   if (info->hash_vaddr != 0) {
      uint32_t header[2];
      if (!read_mem(m, info->hash_vaddr, header, sizeof(header)) || header[0] == 0)
         return NULL;
      const uint32_t nbucket = header[0], nchain = header[1];
      uint32_t index;
      if (!read_mem(m, info->hash_vaddr + 8 + (uint64_t)(sysv_hash(name) % nbucket) * 4, &index, 4))
         return NULL;
      for (uint32_t steps = 0; index != 0 && index < nchain && steps < nchain; steps++) {
         void *found = match(m, index, name);
         if (found != NULL)
            return found;
         if (!read_mem(m, info->hash_vaddr + 8 + (uint64_t)nbucket * 4 + (uint64_t)index * 4, &index,
                       4))
            return NULL;
      }
   }
   return NULL;
}

uintptr_t
akeno_elf_base(const akeno_elf_module *m)
{
   return m != NULL ? m->bias : 0;
}

size_t
akeno_elf_span(const akeno_elf_module *m)
{
   return m != NULL ? m->span : 0;
}

unsigned
akeno_elf_page_protection(const akeno_elf_module *m, const void *address)
{
   if (m == NULL || (uintptr_t)address < (uintptr_t)m->region.base)
      return AKENO_PROT_NONE;
   const size_t into = (size_t)((uintptr_t)address - (uintptr_t)m->region.base);
   if (into >= m->span)
      return AKENO_PROT_NONE;
   return m->page_have[into / m->page_size];
}

void
akeno_elf_unload(akeno_elf_module *m)
{
   if (m == NULL)
      return;
   if (m->constructed)
      run_destructors(m);
   free_module(m);
}
