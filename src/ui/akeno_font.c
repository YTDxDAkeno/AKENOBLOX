/*
 * AkenoBlox PS5 - the launcher's font: font8x8's basic Latin table.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The glyphs are third_party/font8x8/font8x8_basic.h, public domain (Daniel
 * Hepper, from IBM's public-domain VGA font); see its README.
 */
#include "akeno_canvas.h"

#include "../../third_party/font8x8/font8x8_basic.h"

const uint8_t *
akeno_font_glyph(char c)
{
   const unsigned char u = (unsigned char)c;
   if (u < 0x20 || u > 0x7e)
      return (const uint8_t *)font8x8_basic['?'];
   return (const uint8_t *)font8x8_basic[u];
}
