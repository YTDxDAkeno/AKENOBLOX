/*
 * AkenoBlox PS5 - the software canvas.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_canvas.h"

#include <string.h>

/* Intersects [x, x + w) x [y, y + h) with the frame; false when empty. */
static bool
clip(const akeno_frame *f, int *x, int *y, int *w, int *h)
{
   if (f == NULL || f->pixels == NULL || *w <= 0 || *h <= 0)
      return false;
   long x0 = *x, y0 = *y, x1 = (long)*x + *w, y1 = (long)*y + *h;
   if (x0 < 0)
      x0 = 0;
   if (y0 < 0)
      y0 = 0;
   if (x1 > (long)f->width)
      x1 = (long)f->width;
   if (y1 > (long)f->height)
      y1 = (long)f->height;
   if (x0 >= x1 || y0 >= y1)
      return false;
   *x = (int)x0;
   *y = (int)y0;
   *w = (int)(x1 - x0);
   *h = (int)(y1 - y0);
   return true;
}

static uint32_t
mix(uint32_t dst, uint32_t src, unsigned alpha)
{
   if (alpha >= 255)
      return src | 0xff000000u;
   if (alpha == 0)
      return dst;
   const unsigned inv = 255 - alpha;
   const unsigned r = (((src >> 16) & 0xff) * alpha + ((dst >> 16) & 0xff) * inv + 127) / 255;
   const unsigned g = (((src >> 8) & 0xff) * alpha + ((dst >> 8) & 0xff) * inv + 127) / 255;
   const unsigned b = ((src & 0xff) * alpha + (dst & 0xff) * inv + 127) / 255;
   return 0xff000000u | (r << 16) | (g << 8) | b;
}

void
akeno_canvas_clear(akeno_frame *f, uint32_t color)
{
   if (f != NULL)
      akeno_canvas_fill(f, 0, 0, (int)f->width, (int)f->height, color);
}

void
akeno_canvas_fill(akeno_frame *f, int x, int y, int w, int h, uint32_t color)
{
   if (!clip(f, &x, &y, &w, &h))
      return;
   for (int row = 0; row < h; row++) {
      uint32_t *p = f->pixels + (size_t)(y + row) * f->stride + (size_t)x;
      for (int col = 0; col < w; col++)
         p[col] = color | 0xff000000u;
   }
}

void
akeno_canvas_blend(akeno_frame *f, int x, int y, int w, int h, uint32_t argb)
{
   const unsigned alpha = argb >> 24;
   if (alpha == 255) {
      akeno_canvas_fill(f, x, y, w, h, argb);
      return;
   }
   if (alpha == 0 || !clip(f, &x, &y, &w, &h))
      return;
   for (int row = 0; row < h; row++) {
      uint32_t *p = f->pixels + (size_t)(y + row) * f->stride + (size_t)x;
      for (int col = 0; col < w; col++)
         p[col] = mix(p[col], argb, alpha);
   }
}

void
akeno_canvas_gradient(akeno_frame *f, int x, int y, int w, int h, uint32_t top, uint32_t bottom)
{
   if (h <= 0)
      return;
   for (int row = 0; row < h; row++) {
      const unsigned t = h > 1 ? (unsigned)(row * 255 / (h - 1)) : 0;
      akeno_canvas_fill(f, x, y + row, w, 1, mix(top, bottom, t));
   }
}

void
akeno_canvas_round_rect(akeno_frame *f, int x, int y, int w, int h, int radius, uint32_t argb)
{
   if (w <= 0 || h <= 0)
      return;
   if (radius * 2 > w)
      radius = w / 2;
   if (radius * 2 > h)
      radius = h / 2;
   if (radius <= 0) {
      akeno_canvas_blend(f, x, y, w, h, argb);
      return;
   }
   const unsigned alpha = argb >> 24;
   /* The middle band, then each corner row with its coverage. */
   akeno_canvas_blend(f, x, y + radius, w, h - 2 * radius, argb);
   for (int row = 0; row < radius; row++) {
      /* Distance from the corner circle's centre to this row's middle. */
      const int dy2 = (2 * (radius - row) - 1) * (2 * (radius - row) - 1); /* (2*dy)^2 */
      for (int col = 0; col < radius; col++) {
         const int dx = 2 * (radius - col) - 1;
         const int d2 = dx * dx + dy2;            /* (2*d)^2 */
         const int r2_in = (2 * radius - 2) * (2 * radius - 2);
         const int r2_out = (2 * radius) * (2 * radius);
         unsigned cover;
         if (d2 <= r2_in)
            cover = 255;
         else if (d2 >= r2_out)
            cover = 0;
         else
            cover = (unsigned)(255 * (r2_out - d2) / (r2_out - r2_in));
         const uint32_t c = (argb & 0x00ffffffu) | ((alpha * cover / 255) << 24);
         akeno_canvas_blend(f, x + col, y + row, 1, 1, c);
         akeno_canvas_blend(f, x + w - 1 - col, y + row, 1, 1, c);
         akeno_canvas_blend(f, x + col, y + h - 1 - row, 1, 1, c);
         akeno_canvas_blend(f, x + w - 1 - col, y + h - 1 - row, 1, 1, c);
      }
      akeno_canvas_blend(f, x + radius, y + row, w - 2 * radius, 1, argb);
      akeno_canvas_blend(f, x + radius, y + h - 1 - row, w - 2 * radius, 1, argb);
   }
}

void
akeno_canvas_outline(akeno_frame *f, int x, int y, int w, int h, int t, uint32_t argb)
{
   if (t <= 0 || w <= 0 || h <= 0)
      return;
   akeno_canvas_blend(f, x, y, w, t, argb);
   akeno_canvas_blend(f, x, y + h - t, w, t, argb);
   akeno_canvas_blend(f, x, y + t, t, h - 2 * t, argb);
   akeno_canvas_blend(f, x + w - t, y + t, t, h - 2 * t, argb);
}

int
akeno_text_width(const char *text, int scale)
{
   if (text == NULL || scale <= 0)
      return 0;
   return (int)strlen(text) * 8 * scale;
}

static int
draw_glyphs(akeno_frame *f, int x, int y, const char *text, size_t count, int scale, uint32_t color)
{
   for (size_t i = 0; i < count; i++) {
      const uint8_t *glyph = akeno_font_glyph(text[i]);
      for (int row = 0; row < 8; row++) {
         const uint8_t bits = glyph[row];
         if (bits == 0)
            continue;
         int col = 0;
         while (col < 8) {
            if (!(bits & (1u << col))) {
               col++;
               continue;
            }
            int run = col;
            while (run < 8 && (bits & (1u << run)))
               run++;
            akeno_canvas_blend(f, x + col * scale, y + row * scale, (run - col) * scale, scale, color);
            col = run;
         }
      }
      x += 8 * scale;
   }
   return x;
}

int
akeno_canvas_text(akeno_frame *f, int x, int y, const char *text, int scale, uint32_t color)
{
   if (text == NULL || scale <= 0)
      return x;
   return draw_glyphs(f, x, y, text, strlen(text), scale, color);
}

int
akeno_canvas_text_ex(akeno_frame *f, int x, int y, const char *text, int scale, uint32_t color,
                     uint32_t shadow, int max_width)
{
   if (text == NULL || scale <= 0)
      return x;
   size_t count = strlen(text);
   bool ellipsis = false;
   const int glyph = 8 * scale;
   if (max_width > 0 && (int)count * glyph > max_width) {
      const int fit = max_width / glyph;
      count = fit > 3 ? (size_t)(fit - 3) : 0;
      ellipsis = fit >= 3;
   }
   if ((shadow >> 24) != 0) {
      const int offset = scale > 1 ? scale / 2 : 1;
      draw_glyphs(f, x + offset, y + offset, text, count, scale, shadow);
      if (ellipsis)
         draw_glyphs(f, x + offset + (int)count * glyph, y + offset, "...", 3, scale, shadow);
   }
   int end = draw_glyphs(f, x, y, text, count, scale, color);
   if (ellipsis)
      end = draw_glyphs(f, end, y, "...", 3, scale, color);
   return end;
}

uint64_t
akeno_frame_hash(const akeno_frame *f)
{
   uint64_t h = 0xcbf29ce484222325u;
   if (f == NULL || f->pixels == NULL)
      return h;
   for (unsigned y = 0; y < f->height; y++)
      for (unsigned x = 0; x < f->width; x++) {
         const uint32_t p = f->pixels[(size_t)y * f->stride + x];
         for (int b = 0; b < 4; b++) {
            h ^= (p >> (8 * b)) & 0xffu;
            h *= 0x100000001b3u;
         }
      }
   return h;
}
