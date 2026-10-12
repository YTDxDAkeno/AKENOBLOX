/*
 * AkenoBlox PS5 - the launcher's display: CPU-drawn frames through VideoOut.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Every step here is the one PS5_Vulkan's src/demo_renderer.cpp took on a
 * console and drew a correct frame with: two 16 MiB buffers in one direct
 * memory allocation of type 3 (write-combined), mapped CPU and GPU
 * read-write (0x33) at 2 MiB alignment; its pixel format, tiling mode 0 at
 * 1920x1080; flip mode 1 (mode 0 was refused); clflush and mfence after the
 * CPU writes. The only difference is that this presents a frame per vertical
 * blank instead of one still image, double-buffered: a buffer is rewritten
 * only after a vblank has passed since it was last flipped away from.
 *
 * VideoOut is the launcher's only. The engine will own the display through
 * RADV's VK_KHR_display swapchain, in a process of its own (docs/DECISIONS.md,
 * ADR-0004), so this process never has to hand the display over.
 */
#include "ps5_internal.h"

#include "graphics/akeno_tiling.h"

#include <ps5platform/kernel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sceVideoOutOpen(int32_t user, int32_t bus, int32_t index, const void *parameter);
int sceVideoOutClose(int32_t handle);
int sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
void sceVideoOutSetBufferAttribute2(void *attribute, uint64_t pixel_format, uint32_t tiling,
                                    uint32_t width, uint32_t height, uint64_t option,
                                    uint32_t dcc_control, uint64_t dcc_clear_color);
struct video_buffer {
   void *data;
   void *metadata;
   void *reserved[2];
};
int sceVideoOutRegisterBuffers2(int32_t handle, int32_t set, int32_t start, struct video_buffer *buffers,
                                int32_t count, void *attribute, int32_t category, void *option);
int sceVideoOutUnregisterBuffers(int32_t handle, int32_t set);
int sceVideoOutSubmitFlip(int32_t handle, int32_t buffer, uint32_t mode, int64_t argument);
int sceVideoOutWaitVblank(int32_t handle);

enum {
   FRAME_BYTES = 0x1000000,
   BUFFERS = 2,
   MEMORY_ALIGNMENT = 0x200000,
   MEMORY_TYPE_WC_GARLIC = 3,
   MAP_PROTECTION = 0x33,
   FLIP_VSYNC = 1,
};
static const uint64_t PIXEL_FORMAT_RGBA8_SRGB = UINT64_C(0x8000000022000000);

static int32_t video = -1;
static void *mapped;
static int64_t physical;
static size_t mapped_bytes;
static unsigned frame_width, frame_height;
static akeno_tile_map *tile_map;
static uint64_t held_serial[BUFFERS];
static int back;
static int64_t flips;

static void
flush_range(void *address, size_t length)
{
   uint8_t *at = address;
   const uint8_t *end = at + length;
   for (; at < end; at += 64)
      __asm__ volatile("clflush (%0)" : : "r"(at) : "memory");
   __asm__ volatile("mfence" ::: "memory");
}

int
akeno_platform_display_open(unsigned width, unsigned height, char *err, size_t err_size)
{
   if (video >= 0)
      return 0;
   if (width != 1920 || height != 1080 || akeno_tiled_size(width, height) > FRAME_BYTES) {
      snprintf(err, err_size, "only 1920x1080 is set up for VideoOut");
      return -1;
   }
   tile_map = malloc(sizeof(*tile_map));
   if (tile_map == NULL || !akeno_tile_map_build(tile_map)) {
      snprintf(err, err_size, "the tile map could not be built");
      free(tile_map);
      tile_map = NULL;
      return -1;
   }
   video = sceVideoOutOpen(0xff, 0, 0, NULL);
   if (video < 0) {
      snprintf(err, err_size, "sceVideoOutOpen: 0x%08x", (unsigned)video);
      return -1;
   }
   mapped_bytes = (size_t)FRAME_BYTES * BUFFERS;
   const int64_t pool = sceKernelGetDirectMemorySize();
   int rc = sceKernelAllocateDirectMemory(0, pool, mapped_bytes, MEMORY_ALIGNMENT,
                                          MEMORY_TYPE_WC_GARLIC, &physical);
   if (rc < 0) {
      snprintf(err, err_size, "sceKernelAllocateDirectMemory: 0x%08x", (unsigned)rc);
      akeno_platform_display_close();
      return -1;
   }
   rc = sceKernelMapDirectMemory(&mapped, mapped_bytes, MAP_PROTECTION, 0, physical, MEMORY_ALIGNMENT);
   if (rc < 0) {
      snprintf(err, err_size, "sceKernelMapDirectMemory: 0x%08x", (unsigned)rc);
      sceKernelReleaseDirectMemory(physical, mapped_bytes);
      physical = 0;
      mapped = NULL;
      akeno_platform_display_close();
      return -1;
   }
   memset(mapped, 0, mapped_bytes);
   flush_range(mapped, mapped_bytes);

   struct video_buffer buffers[BUFFERS];
   memset(buffers, 0, sizeof(buffers));
   for (int i = 0; i < BUFFERS; i++)
      buffers[i].data = (uint8_t *)mapped + (size_t)i * FRAME_BYTES;
   uint8_t attribute[80];
   memset(attribute, 0, sizeof(attribute));
   (void)sceVideoOutSetFlipRate(video, 0);
   sceVideoOutSetBufferAttribute2(attribute, PIXEL_FORMAT_RGBA8_SRGB, 0, width, height, 0, 0, 0);
   rc = sceVideoOutRegisterBuffers2(video, 0, 0, buffers, BUFFERS, attribute, 0, NULL);
   if (rc < 0) {
      snprintf(err, err_size, "sceVideoOutRegisterBuffers2: 0x%08x", (unsigned)rc);
      akeno_platform_display_close();
      return -1;
   }
   frame_width = width;
   frame_height = height;
   memset(held_serial, 0xff, sizeof(held_serial));
   back = 0;
   return 0;
}

int
akeno_platform_display_present(const akeno_frame *frame)
{
   if (video < 0 || frame == NULL || frame->width != frame_width || frame->height != frame_height)
      return -1;
   uint32_t *target = (uint32_t *)((uint8_t *)mapped + (size_t)back * FRAME_BYTES);
   if (held_serial[back] != frame->serial) {
      /* The demo's format stores bytes R, G, B, A; frames are B, G, R, A. */
      akeno_tile_copy(tile_map, frame->pixels, frame->width, frame->height, frame->stride, target, true);
      flush_range(target, akeno_tiled_size(frame_width, frame_height));
      held_serial[back] = frame->serial;
   }
   const int rc = sceVideoOutSubmitFlip(video, back, FLIP_VSYNC, ++flips);
   if (rc < 0) {
      fprintf(stderr, "display: sceVideoOutSubmitFlip: 0x%08x\n", (unsigned)rc);
      return -1;
   }
   /* The flip happens at this vblank; after it the other buffer is free. */
   (void)sceVideoOutWaitVblank(video);
   back ^= 1;
   return 0;
}

void
akeno_platform_display_close(void)
{
   if (video >= 0) {
      /* 0x80290009 (busy) is expected while a flip is pending; others carry on. */
      (void)sceVideoOutUnregisterBuffers(video, 0);
      (void)sceVideoOutClose(video);
      video = -1;
   }
   if (mapped != NULL) {
      sceKernelMunmap(mapped, mapped_bytes);
      mapped = NULL;
   }
   if (physical != 0) {
      sceKernelReleaseDirectMemory(physical, mapped_bytes);
      physical = 0;
   }
   free(tile_map);
   tile_map = NULL;
}
