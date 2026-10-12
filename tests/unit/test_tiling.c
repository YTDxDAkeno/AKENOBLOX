/*
 * AkenoBlox PS5 - the VideoOut tile layout, checked on the host.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_test.h"

#include "graphics/akeno_tiling.h"

#include <stdlib.h>

static akeno_tile_map map;

static void
test_map_is_a_bijection(void)
{
   CHECK(akeno_tile_map_build(&map));
   CHECK_EQ_INT(akeno_tile_offset(0, 0), 0);
   /* Every offset is a four-byte slot inside the tile. */
   for (unsigned y = 0; y < 128; y += 7)
      for (unsigned x = 0; x < 128; x += 5)
         CHECK((akeno_tile_offset(x, y) & 3u) == 0 && akeno_tile_offset(x, y) < AKENO_TILE_BYTES);
   /* The inverse round-trips. */
   for (unsigned slot = 0; slot < AKENO_TILE_SLOTS; slot += 97)
      CHECK_EQ_INT(akeno_tile_offset(map.x[slot], map.y[slot]) / 4, slot);
}

static void
test_sizes(void)
{
   /* 1920x1080: 15 tiles across, 9 down (1080 = 8 x 128 + 56). */
   CHECK_EQ_INT(akeno_tiled_size(1920, 1080), 15 * 9 * 65536);
   CHECK(akeno_tiled_size(1920, 1080) <= 0x1000000); /* fits the 16 MiB buffer */
   CHECK_EQ_INT(akeno_tiled_size(128, 128), 65536);
   CHECK_EQ_INT(akeno_tiled_size(129, 1), 2 * 65536);
}

/* Every pixel lands exactly where the demo renderer's scatter would put it. */
static void
test_copy_matches_scatter(void)
{
   REQUIRE(akeno_tile_map_build(&map));
   const unsigned w = 300, h = 140;
   uint32_t *pixels = malloc((size_t)w * h * 4);
   const size_t bytes = akeno_tiled_size(w, h);
   uint32_t *tiled = malloc(bytes), *expected = malloc(bytes);
   REQUIRE(pixels && tiled && expected);
   for (unsigned i = 0; i < w * h; i++)
      pixels[i] = 0xff000000u | (i * 2654435761u >> 8);
   for (size_t i = 0; i < bytes / 4; i++)
      expected[i] = 0xff000000u;
   const unsigned across = (w + 127) / 128;
   for (unsigned y = 0; y < h; y++)
      for (unsigned x = 0; x < w; x++) {
         const size_t tile = (size_t)(y / 128) * across + x / 128;
         expected[tile * AKENO_TILE_SLOTS + akeno_tile_offset(x % 128, y % 128) / 4] = pixels[y * w + x];
      }
   akeno_tile_copy(&map, pixels, w, h, w, tiled, false);
   CHECK(memcmp(tiled, expected, bytes) == 0);
   /* With red and blue swapped, only those bytes move. */
   akeno_tile_copy(&map, pixels, w, h, w, tiled, true);
   const uint32_t a = expected[1], b = tiled[1];
   CHECK((a & 0xff00ff00u) == (b & 0xff00ff00u));
   CHECK(((a >> 16) & 0xff) == (b & 0xff) && (a & 0xff) == ((b >> 16) & 0xff));
   free(pixels);
   free(tiled);
   free(expected);
}

static const akeno_test_case cases[] = {
   {"map_is_a_bijection", test_map_is_a_bijection},
   {"sizes", test_sizes},
   {"copy_matches_scatter", test_copy_matches_scatter},
};

AKENO_TEST_MAIN(cases)
