/*
 * AkenoBlox PS5 - code memory: what the platform gives a loader to copy into.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The one memory contract the ELF loader (and later the bionic linker port)
 * needs from a platform. On a host it is mmap and mprotect; on the console it
 * is ps5platform/exec.h, because there execute is refused at map time and
 * granted by sceKernelMprotect afterwards, in 16 KiB pages.
 */
#ifndef AKENO_MEM_H
#define AKENO_MEM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Protection requests. On the console only read-write and read-execute exist
 * for code memory (ps5_exec_protect), so a platform may apply more than was
 * asked; it reports what it applied. */
#define AKENO_PROT_NONE 0u
#define AKENO_PROT_READ 0x1u
#define AKENO_PROT_WRITE 0x2u
#define AKENO_PROT_EXEC 0x4u

/* Reserve flags. */
#define AKENO_MEM_NEED_WX 0x1u /* some page must be writable and executable at once */

typedef struct akeno_mem_region {
   void *base;
   size_t size;
   uintptr_t token[4]; /* the platform's own bookkeeping */
} akeno_mem_region;

typedef struct akeno_mem_ops {
   /* The granularity of protect(); a power of two. */
   size_t page_size;
   /* Readable, writable, zero-filled, aligned to align (a power of two no
    * smaller than page_size). Returns 0, or a non-zero platform code. */
   int (*reserve)(void *ctx, size_t size, size_t align, unsigned flags, akeno_mem_region *out);
   /* [offset, offset + length) of the region, both page-aligned. *applied
    * receives the protection actually in force. Returns 0 or non-zero. */
   int (*protect)(void *ctx, const akeno_mem_region *region, size_t offset, size_t length,
                  unsigned prot, unsigned *applied);
   void (*release)(void *ctx, akeno_mem_region *region);
   void *ctx;
} akeno_mem_ops;

#ifdef __cplusplus
}
#endif

#endif /* AKENO_MEM_H */
