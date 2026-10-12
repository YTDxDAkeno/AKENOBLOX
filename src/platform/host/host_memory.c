/*
 * AkenoBlox PS5 - code memory on a POSIX host: mmap and mprotect.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The host's answer to the contract in akeno_mem.h, used by the tests and the
 * host build. It can pretend its pages are larger than the kernel's
 * (akeno_host_code_memory_paged), which is how the tests run the loader at the
 * console's 16 KiB page size on a 4 KiB host: every range it protects is then
 * a multiple of 16 KiB, as the console's would be.
 */
#define _DEFAULT_SOURCE
#include "akeno_host.h"

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

static size_t
system_page(void)
{
   const long page = sysconf(_SC_PAGESIZE);
   return page > 0 ? (size_t)page : 4096;
}

static int
host_reserve(void *ctx, size_t size, size_t align, unsigned flags, akeno_mem_region *out)
{
   (void)ctx;
   (void)flags;
   if (size == 0 || align == 0 || (align & (align - 1)) != 0 || size > SIZE_MAX - align)
      return EINVAL;
   const size_t page = system_page();
   if (align < page)
      align = page;
   const size_t total = size + align;
   uint8_t *raw = mmap(NULL, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   if (raw == MAP_FAILED)
      return errno ? errno : ENOMEM;
   uint8_t *base = (uint8_t *)(((uintptr_t)raw + align - 1) & ~(uintptr_t)(align - 1));
   if (base > raw)
      munmap(raw, (size_t)(base - raw));
   const size_t tail = (size_t)((raw + total) - (base + size));
   if (tail > 0)
      munmap(base + size, tail);
   if (mprotect(base, size, PROT_READ | PROT_WRITE) != 0) {
      const int error = errno;
      munmap(base, size);
      return error ? error : EACCES;
   }
   out->base = base;
   out->size = size;
   out->token[0] = 0;
   return 0;
}

static int
host_protect(void *ctx, const akeno_mem_region *region, size_t offset, size_t length,
             unsigned prot, unsigned *applied)
{
   (void)ctx;
   if (region == NULL || region->base == NULL || offset > region->size ||
       length > region->size - offset)
      return EINVAL;
   int native = PROT_NONE;
   if (prot & AKENO_PROT_READ)
      native |= PROT_READ;
   if (prot & AKENO_PROT_WRITE)
      native |= PROT_WRITE;
   if (prot & AKENO_PROT_EXEC)
      native |= PROT_EXEC;
   if (mprotect((uint8_t *)region->base + offset, length, native) != 0)
      return errno ? errno : EACCES;
   if (applied != NULL)
      *applied = prot;
   return 0;
}

static void
host_release(void *ctx, akeno_mem_region *region)
{
   (void)ctx;
   if (region == NULL || region->base == NULL)
      return;
   munmap(region->base, region->size);
   region->base = NULL;
   region->size = 0;
}

const akeno_mem_ops *
akeno_host_code_memory_paged(size_t page_size)
{
   /* A handful of fixed sizes: the ops are returned by pointer and must outlive
    * every module loaded through them. */
   static akeno_mem_ops sizes[4];
   static const size_t known[4] = {0, 4096, 16384, 65536};
   size_t slot = 0;
   for (size_t i = 1; i < 4; i++)
      if (known[i] == page_size)
         slot = i;
   akeno_mem_ops *ops = &sizes[slot];
   const size_t page = system_page();
   ops->page_size = slot == 0 ? page : (known[slot] < page ? page : known[slot]);
   ops->reserve = host_reserve;
   ops->protect = host_protect;
   ops->release = host_release;
   ops->ctx = NULL;
   return ops;
}

const akeno_mem_ops *
akeno_host_code_memory(void)
{
   return akeno_host_code_memory_paged(0);
}
