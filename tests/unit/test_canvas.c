/*
 * AkenoBlox PS5 - canvas and screen tests.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Drawing is checked by what it guarantees (clipping, determinism, the
 * selection drawn where the menu says it is) rather than by golden images:
 * the header prints the commit, so a whole-frame hash changes every commit.
 */
#include "akeno_test.h"

#include "ui/akeno_canvas.h"
#include "ui/akeno_screens.h"

#include <stdlib.h>

static akeno_frame
frame(unsigned w, unsigned h)
{
   akeno_frame f = {calloc((size_t)w * h, 4), w, h, w, 0};
   return f;
}

static void
test_fill_clips(void)
{
   akeno_frame f = frame(16, 8);
   REQUIRE(f.pixels != NULL);
   akeno_canvas_fill(&f, -100, -100, 1000, 1000, AKENO_RGB(1, 2, 3)); /* covers all, clipped */
   for (unsigned i = 0; i < 16 * 8; i++)
      CHECK(f.pixels[i] == AKENO_RGB(1, 2, 3));
   akeno_canvas_fill(&f, 15, 7, 5, 5, AKENO_RGB(9, 9, 9));
   CHECK(f.pixels[7 * 16 + 15] == AKENO_RGB(9, 9, 9));
   CHECK(f.pixels[7 * 16 + 14] == AKENO_RGB(1, 2, 3));
   akeno_canvas_fill(&f, 20, 20, 5, 5, 0); /* wholly outside: nothing */
   akeno_canvas_fill(&f, 2, 2, 0, 5, 0);   /* empty */
   akeno_canvas_fill(&f, 2, 2, -3, 5, 0);  /* negative */
   CHECK(f.pixels[2 * 16 + 2] == AKENO_RGB(1, 2, 3));
   free(f.pixels);
}

static void
test_blend(void)
{
   akeno_frame f = frame(4, 4);
   REQUIRE(f.pixels != NULL);
   akeno_canvas_clear(&f, AKENO_RGB(0, 0, 0));
   akeno_canvas_blend(&f, 0, 0, 4, 4, AKENO_ARGB(128, 255, 255, 255));
   const uint32_t p = f.pixels[5];
   CHECK(((p >> 16) & 0xff) >= 127 && ((p >> 16) & 0xff) <= 129);
   akeno_canvas_blend(&f, 0, 0, 4, 4, AKENO_ARGB(0, 255, 0, 0)); /* transparent: no change */
   CHECK(f.pixels[5] == p);
   free(f.pixels);
}

static void
test_text(void)
{
   akeno_frame f = frame(200, 40);
   REQUIRE(f.pixels != NULL);
   akeno_canvas_clear(&f, AKENO_RGB(0, 0, 0));
   CHECK_EQ_INT(akeno_text_width("Hello", 2), 80);
   const int end = akeno_canvas_text(&f, 4, 4, "Hi!", 2, AKENO_RGB(255, 255, 255));
   CHECK_EQ_INT(end, 4 + 3 * 16);
   int lit = 0;
   for (unsigned i = 0; i < 200 * 40; i++)
      lit += f.pixels[i] == AKENO_RGB(255, 255, 255);
   CHECK(lit > 50);
   /* Cut to fit with an ellipsis, and nothing past max_width. */
   akeno_canvas_clear(&f, AKENO_RGB(0, 0, 0));
   akeno_canvas_text_ex(&f, 0, 0, "a long line of text", 1, AKENO_RGB(255, 255, 255), 0, 80);
   for (unsigned y = 0; y < 40; y++)
      for (unsigned x = 80; x < 200; x++)
         CHECK(f.pixels[y * 200 + x] == AKENO_RGB(0, 0, 0));
   /* Characters outside ASCII draw something rather than nothing. */
   CHECK(akeno_font_glyph('\x01') == akeno_font_glyph('?'));
   CHECK(akeno_font_glyph('A')[0] == 0x0C);
   free(f.pixels);
}

static void
test_round_rect_stays_inside(void)
{
   akeno_frame f = frame(64, 64);
   REQUIRE(f.pixels != NULL);
   akeno_canvas_clear(&f, AKENO_RGB(0, 0, 0));
   akeno_canvas_round_rect(&f, 8, 8, 48, 48, 12, AKENO_RGB(255, 0, 0));
   CHECK(f.pixels[8 * 64 + 8] != AKENO_RGB(255, 0, 0));  /* the corner is cut */
   CHECK(f.pixels[32 * 64 + 32] == AKENO_RGB(255, 0, 0)); /* the middle is filled */
   CHECK(f.pixels[7 * 64 + 32] == AKENO_RGB(0, 0, 0));    /* nothing outside */
   CHECK(f.pixels[56 * 64 + 32] == AKENO_RGB(0, 0, 0));
   akeno_canvas_round_rect(&f, -30, -30, 40, 40, 100, AKENO_RGB(0, 255, 0)); /* clipped, big radius */
   free(f.pixels);
}

static void
test_every_screen_draws(void)
{
   akeno_frame f = frame(AKENO_UI_WIDTH, AKENO_UI_HEIGHT);
   REQUIRE(f.pixels != NULL);
   akeno_settings settings;
   akeno_settings_defaults(&settings);
   akeno_menu menu;
   akeno_menu_init(&menu, &settings, 0);
   akeno_report report;
   akeno_report_clear(&report);
   for (int i = 0; i < 40; i++)
      akeno_report_add(&report, i < 20 ? "One" : "Two", (akeno_row_status)(i % 5), "label",
                       "value %d", i);
   akeno_package_check package = {.searched = true, .found = false};
   snprintf(package.path, sizeof(package.path), "/data/akenoblox/roblox/lib/x86_64/libroblox.so");
   akeno_pad_state pad = {.connected = true, .buttons = AKENO_BTN_CROSS, .left_x = 0.5f, .r2 = 1.0f};
   akeno_gamepad_mapping mapping;
   akeno_gamepad_mapping_default(&mapping);
   akeno_android_gamepad gamepad;
   akeno_android_gamepad_from_pad(&pad, &mapping, 10, &gamepad);
   akeno_view view = {.menu = &menu, .report = &report, .package = &package, .pad = &pad,
                      .gamepad = &gamepad, .mapping = &mapping, .toast = "A notice"};
   uint64_t hashes[AKENO_SCREEN_COUNT];
   for (int s = 0; s < AKENO_SCREEN_COUNT; s++) {
      menu.screen = (akeno_screen)s;
      int rows = -1, visible = -1;
      akeno_screens_draw(&f, &view, &rows, &visible);
      hashes[s] = akeno_frame_hash(&f);
      CHECK(rows >= 0 && visible >= 1);
      /* Drawing the same view twice draws the same pixels. */
      akeno_screens_draw(&f, &view, &rows, &visible);
      CHECK(akeno_frame_hash(&f) == hashes[s]);
   }
   for (int a = 0; a < AKENO_SCREEN_COUNT; a++)
      for (int b = 0; b < a; b++)
         CHECK(hashes[a] != hashes[b]); /* every screen looks different */
   /* The Diagnostics list knows its length. */
   menu.screen = AKENO_SCREEN_DIAGNOSTICS;
   int rows, visible;
   akeno_screens_draw(&f, &view, &rows, &visible);
   CHECK_EQ_INT(rows, 40);
   free(f.pixels);
}

static void
test_selection_drawn_where_the_menu_says(void)
{
   akeno_frame f = frame(AKENO_UI_WIDTH, AKENO_UI_HEIGHT);
   REQUIRE(f.pixels != NULL);
   akeno_settings settings;
   akeno_settings_defaults(&settings);
   akeno_menu menu;
   akeno_menu_init(&menu, &settings, 0);
   akeno_view view = {.menu = &menu};
   const uint32_t accent = AKENO_RGB(0x2e, 0x7d, 0xff);
   for (int selected = 0; selected < AKENO_HOME_COUNT; selected++) {
      menu.home_index = selected;
      akeno_screens_draw(&f, &view, NULL, NULL);
      for (int item = 0; item < AKENO_HOME_COUNT; item++) {
         /* A point inside the item's box, clear of its text. */
         const int x = 72 + 500, y = 168 + item * (80 + 12) + 8;
         const bool lit = f.pixels[(size_t)y * AKENO_UI_WIDTH + (size_t)x] == accent;
         CHECK(lit == (item == selected));
      }
   }
   free(f.pixels);
}

static const akeno_test_case cases[] = {
   {"fill_clips", test_fill_clips},
   {"blend", test_blend},
   {"text", test_text},
   {"round_rect_stays_inside", test_round_rect_stays_inside},
   {"every_screen_draws", test_every_screen_draws},
   {"selection_drawn_where_the_menu_says", test_selection_drawn_where_the_menu_says},
};

AKENO_TEST_MAIN(cases)
