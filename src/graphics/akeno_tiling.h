/*
 * AkenoBlox PS5 - the display's tiled layout, for CPU-drawn frames.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * VideoOut's tiling mode 0 stores 128x128 blocks of four-byte pixels in 64 KiB
 * tiles, the colour block's SW_64KB_R_X with no pipe or bank swizzle
 * (ps5platform/videoout.h). The address of a pixel inside a tile is
 * PS5_Vulkan's src/demo_renderer.cpp tiled_byte_offset, the function its
 * console run drew a correct frame with; this file implements the same
 * mapping and its inverse, so a frame is written to the tiles in address
 * order (sequential writes to write-combined memory) rather than scattered.
 */
#ifndef AKENO_TILING_H
#define AKENO_TILING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
   AKENO_TILE_PIXELS = 128,
   AKENO_TILE_BYTES = 65536,
   AKENO_TILE_SLOTS = AKENO_TILE_BYTES / 4,
};

/* The byte offset of (x, y) within its 64 KiB tile, x and y in 0..127. */
uint32_t akeno_tile_offset(unsigned x, unsigned y);

/* The inverse: for every four-byte slot of a tile, the pixel it holds.
 * Returns false if the mapping is not a bijection (it must be). */
typedef struct akeno_tile_map {
   uint8_t x[AKENO_TILE_SLOTS];
   uint8_t y[AKENO_TILE_SLOTS];
} akeno_tile_map;
bool akeno_tile_map_build(akeno_tile_map *map);

/* Bytes a tiled surface of width x height needs: whole tiles. */
size_t akeno_tiled_size(unsigned width, unsigned height);

/* Writes a linear 0xAARRGGBB frame into a tiled surface, in address order.
 * swap_red_blue writes bytes R, G, B, A instead of B, G, R, A. Pixels past
 * the frame's edge are written black. */
void akeno_tile_copy(const akeno_tile_map *map, const uint32_t *pixels, unsigned width,
                     unsigned height, unsigned stride, uint32_t *tiled, bool swap_red_blue);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_TILING_H */
