/*
 * AkenoBlox PS5 - the display's tiled layout.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_tiling.h"

#include <string.h>

uint32_t
akeno_tile_offset(unsigned x, unsigned y)
{
   /* PS5_Vulkan src/demo_renderer.cpp, tiled_byte_offset, within one tile. */
   return ((y << 4) & 0x70u) ^ ((y << 5) & 0xf00u) ^ ((y << 9) & 0x1000u) ^ ((y << 8) & 0x4000u) ^
          ((x << 2) & 0xcu) ^ ((x << 5) & 0x380u) ^ ((x << 4) & 0x400u) ^ ((x << 6) & 0x800u) ^
          ((x << 9) & 0xa000u);
}

bool
akeno_tile_map_build(akeno_tile_map *map)
{
   static uint8_t seen[AKENO_TILE_SLOTS];
   memset(seen, 0, sizeof(seen));
   for (unsigned y = 0; y < AKENO_TILE_PIXELS; y++)
      for (unsigned x = 0; x < AKENO_TILE_PIXELS; x++) {
         const uint32_t offset = akeno_tile_offset(x, y);
         if ((offset & 3u) != 0 || offset >= AKENO_TILE_BYTES)
            return false;
         const uint32_t slot = offset / 4;
         if (seen[slot])
            return false;
         seen[slot] = 1;
         map->x[slot] = (uint8_t)x;
         map->y[slot] = (uint8_t)y;
      }
   return true;
}

size_t
akeno_tiled_size(unsigned width, unsigned height)
{
   const size_t across = (width + AKENO_TILE_PIXELS - 1) / AKENO_TILE_PIXELS;
   const size_t down = (height + AKENO_TILE_PIXELS - 1) / AKENO_TILE_PIXELS;
   return across * down * AKENO_TILE_BYTES;
}

void
akeno_tile_copy(const akeno_tile_map *map, const uint32_t *pixels, unsigned width, unsigned height,
                unsigned stride, uint32_t *tiled, bool swap_red_blue)
{
   const unsigned across = (width + AKENO_TILE_PIXELS - 1) / AKENO_TILE_PIXELS;
   const unsigned down = (height + AKENO_TILE_PIXELS - 1) / AKENO_TILE_PIXELS;
   for (unsigned ty = 0; ty < down; ty++)
      for (unsigned tx = 0; tx < across; tx++) {
         uint32_t *tile = tiled + ((size_t)ty * across + tx) * AKENO_TILE_SLOTS;
         const unsigned x0 = tx * AKENO_TILE_PIXELS, y0 = ty * AKENO_TILE_PIXELS;
         for (unsigned slot = 0; slot < AKENO_TILE_SLOTS; slot++) {
            const unsigned x = x0 + map->x[slot], y = y0 + map->y[slot];
            uint32_t p = 0xff000000u;
            if (x < width && y < height) {
               p = pixels[(size_t)y * stride + x];
               if (swap_red_blue)
                  p = (p & 0xff00ff00u) | ((p >> 16) & 0xffu) | ((p & 0xffu) << 16);
            }
            tile[slot] = p;
         }
      }
}
