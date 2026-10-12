/*
 * AkenoBlox PS5 - a software canvas: the launcher's drawing.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The launcher draws on the CPU into an akeno_frame, which the platform
 * presents (a PPM on the host, VideoOut on the console). Drawing on the CPU
 * keeps the launcher independent of the GPU driver the engine will need, and
 * makes every screen testable on a host by hashing its pixels. Every call
 * clips to the frame; nothing writes outside it.
 */
#ifndef AKENO_CANVAS_H
#define AKENO_CANVAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "akeno_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AKENO_RGB(r, g, b) (0xff000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define AKENO_ARGB(a, r, g, b)                                                                     \
   (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

void akeno_canvas_clear(akeno_frame *f, uint32_t color);
/* Opaque fill. */
void akeno_canvas_fill(akeno_frame *f, int x, int y, int w, int h, uint32_t color);
/* Alpha-blended fill (color's alpha byte). */
void akeno_canvas_blend(akeno_frame *f, int x, int y, int w, int h, uint32_t argb);
/* A vertical gradient, opaque. */
void akeno_canvas_gradient(akeno_frame *f, int x, int y, int w, int h, uint32_t top,
                           uint32_t bottom);
/* A rounded rectangle, alpha-blended, its corners anti-aliased. */
void akeno_canvas_round_rect(akeno_frame *f, int x, int y, int w, int h, int radius, uint32_t argb);
/* A rectangle's outline, thickness pixels wide, alpha-blended. */
void akeno_canvas_outline(akeno_frame *f, int x, int y, int w, int h, int thickness, uint32_t argb);

/* Text in the 8x8 font scaled by scale (1..8). Returns the x after the last
 * glyph. Characters outside printable ASCII draw as '?'. */
int akeno_canvas_text(akeno_frame *f, int x, int y, const char *text, int scale, uint32_t color);
/* As above, with a one-pixel-per-scale shadow, and cut to max_width with
 * "..." when it would not fit (max_width <= 0: no limit). */
int akeno_canvas_text_ex(akeno_frame *f, int x, int y, const char *text, int scale, uint32_t color,
                         uint32_t shadow, int max_width);
int akeno_text_width(const char *text, int scale);

/* FNV-1a over the visible pixels: what host tests compare. */
uint64_t akeno_frame_hash(const akeno_frame *f);

/* 8 rows of a glyph, least significant bit leftmost (font8x8's encoding). */
const uint8_t *akeno_font_glyph(char c);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_CANVAS_H */
