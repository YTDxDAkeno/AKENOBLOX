/*
 * AkenoBlox PS5 - code memory on the console: ps5platform/exec.h.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the console measured (PS5_PayloadSDK platform/docs/PROBE.md, quoted
 * in exec.h): direct memory mapped read-write and then given execute with
 * sceKernelMprotect runs; execute asked for at map time is refused; an
 * unplaced mapping lands in the GPU driver's window; pages are 16 KiB; a
 * protection change costs about 26 us. So a library is copied into a
 * PS5_EXEC_TOGGLED region, read-write, and its pages are toggled to
 * read-execute once relocated. exec.h toggles between read-write and
 * read-execute only: a read-only page stays read-write here, and the loader
 * is told so through `applied`.
 *
 * A library whose segments share pages (4 KiB alignment on 16 KiB pages)
 * needs some pages writable and executable at once; it gets an untoggled
 * region, which exec.h makes read, write and execute throughout.
 */
#include "ps5_internal.h"

#include <ps5platform/exec.h>
#include <ps5platform/kernel.h>

#include <stdlib.h>
#include <string.h>

enum { TOKEN_REGION = 0, TOKEN_OFFSET = 1, TOKEN_TOGGLED = 2 };

static int
ps5_reserve(void *ctx, size_t size, size_t align, unsigned flags, akeno_mem_region *out)
{
   (void)ctx;
   if (size == 0 || align == 0 || (align & (align - 1)) != 0)
      return PS5_EXEC_BAD_REQUEST;
   /* exec.h places regions in 64 KiB direct-memory units; ask for slack when
    * the library wants more alignment than that. */
   const size_t slack = align > PS5_KERNEL_DIRECT_ALIGNMENT ? align : 0;
   struct ps5_exec_region *region = calloc(1, sizeof(*region));
   if (region == NULL)
      return PS5_EXEC_BAD_REQUEST;
   const bool toggled = (flags & AKENO_MEM_NEED_WX) == 0;
   const struct ps5_exec_request request = {
      .bytes = size + slack,
      .flags = toggled ? PS5_EXEC_TOGGLED : 0u,
   };
   const int rc = ps5_exec_alloc(&request, region);
   if (rc != 0) {
      free(region);
      return rc;
   }
   const uintptr_t start = (uintptr_t)region->base;
   const uintptr_t base = (start + align - 1) & ~(uintptr_t)(align - 1);
   if (base + size > start + region->bytes) {
      ps5_exec_free(region);
      free(region);
      return PS5_EXEC_NO_PLACE;
   }
   /* Written through write_view: base unless a dual view was asked for. */
   memset((uint8_t *)region->write_view + (base - start), 0, size);
   out->base = (void *)base;
   out->size = size;
   out->token[TOKEN_REGION] = (uintptr_t)region;
   out->token[TOKEN_OFFSET] = base - start;
   out->token[TOKEN_TOGGLED] = toggled;
   return 0;
}

static int
ps5_protect(void *ctx, const akeno_mem_region *r, size_t offset, size_t length, unsigned prot,
            unsigned *applied)
{
   (void)ctx;
   const struct ps5_exec_region *region = (const struct ps5_exec_region *)r->token[TOKEN_REGION];
   if (region == NULL || offset > r->size || length > r->size - offset)
      return PS5_EXEC_BAD_REQUEST;
   if (!r->token[TOKEN_TOGGLED]) {
      *applied = AKENO_PROT_READ | AKENO_PROT_WRITE | AKENO_PROT_EXEC;
      return 0;
   }
   const bool writable = (prot & AKENO_PROT_EXEC) == 0;
   const int rc = ps5_exec_protect(region, r->token[TOKEN_OFFSET] + offset, length, writable);
   if (rc != 0)
      return rc;
   *applied = writable ? (AKENO_PROT_READ | AKENO_PROT_WRITE) : (AKENO_PROT_READ | AKENO_PROT_EXEC);
   return 0;
}

static void
ps5_release(void *ctx, akeno_mem_region *r)
{
   (void)ctx;
   struct ps5_exec_region *region = (struct ps5_exec_region *)r->token[TOKEN_REGION];
   if (region != NULL) {
      ps5_exec_free(region);
      free(region);
   }
   memset(r, 0, sizeof(*r));
}

const akeno_mem_ops *
akeno_ps5_code_memory(void)
{
   static const akeno_mem_ops ops = {
      .page_size = PS5_KERNEL_PAGE_SIZE,
      .reserve = ps5_reserve,
      .protect = ps5_protect,
      .release = ps5_release,
      .ctx = NULL,
   };
   return &ops;
}
