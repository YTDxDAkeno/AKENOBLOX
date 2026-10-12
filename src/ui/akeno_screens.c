/*
 * AkenoBlox PS5 - drawing the launcher's screens.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Dark, blue-accented, readable from a sofa: body text is the 8x8 font at
 * three times its size (24 px at 1080p). The PlayStation button glyphs are
 * drawn from rectangles here, not copied from anyone's artwork.
 */
#include "akeno_screens.h"

#include "akeno_version.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------- palette */

#define BG_TOP AKENO_RGB(0x05, 0x09, 0x14)
#define BG_BOTTOM AKENO_RGB(0x0b, 0x16, 0x31)
#define PANEL AKENO_ARGB(200, 0x0f, 0x18, 0x2e)
#define PANEL_SOFT AKENO_ARGB(130, 0x14, 0x20, 0x3c)
#define ACCENT AKENO_RGB(0x2e, 0x7d, 0xff)
#define ACCENT_GLOW AKENO_ARGB(70, 0x2e, 0x7d, 0xff)
#define TEXT AKENO_RGB(0xe8, 0xee, 0xf9)
#define TEXT_DIM AKENO_RGB(0x8c, 0x9b, 0xb8)
#define SHADOW AKENO_ARGB(150, 0, 0, 0)
#define OK_GREEN AKENO_RGB(0x3d, 0xdc, 0x84)
#define WARN_AMBER AKENO_RGB(0xff, 0xb0, 0x20)
#define FAIL_RED AKENO_RGB(0xff, 0x4d, 0x5e)
#define CROSS_BLUE AKENO_RGB(0x7c, 0xb2, 0xff)
#define CIRCLE_RED AKENO_RGB(0xff, 0x6b, 0x81)
#define SQUARE_PINK AKENO_RGB(0xf2, 0xa2, 0xd8)
#define TRIANGLE_GREEN AKENO_RGB(0x4f, 0xd1, 0xa5)

#define NAV_X 72
#define NAV_Y 168
#define NAV_W 520
#define NAV_H 80
#define NAV_GAP 12
#define PANEL_X 640
#define PANEL_Y 168
#define PANEL_W 1208
#define PANEL_H 792
#define BODY_X (PANEL_X + 48)
#define BODY_W (PANEL_W - 96)
#define FOOTER_Y 1000

/* -------------------------------------------------------------- glyphs */

typedef enum glyph { GLYPH_CROSS, GLYPH_CIRCLE, GLYPH_SQUARE, GLYPH_TRIANGLE, GLYPH_L1R1 } glyph;

static void
draw_glyph(akeno_frame *f, int x, int y, int size, glyph g)
{
   switch (g) {
   case GLYPH_CROSS:
      for (int i = 0; i < size; i++) {
         akeno_canvas_fill(f, x + i, y + i, 4, 4, CROSS_BLUE);
         akeno_canvas_fill(f, x + size - 1 - i, y + i, 4, 4, CROSS_BLUE);
      }
      break;
   case GLYPH_CIRCLE:
      akeno_canvas_round_rect(f, x, y, size + 3, size + 3, (size + 3) / 2, CIRCLE_RED);
      akeno_canvas_round_rect(f, x + 4, y + 4, size - 5, size - 5, (size - 5) / 2, BG_BOTTOM);
      break;
   case GLYPH_SQUARE:
      akeno_canvas_outline(f, x + 2, y + 2, size, size, 4, SQUARE_PINK);
      break;
   case GLYPH_TRIANGLE:
      for (int row = 0; row < size; row += 2) {
         const int half = row / 2;
         akeno_canvas_fill(f, x + size / 2 - half, y + row, 4, 4, TRIANGLE_GREEN);
         akeno_canvas_fill(f, x + size / 2 + half, y + row, 4, 4, TRIANGLE_GREEN);
      }
      akeno_canvas_fill(f, x, y + size - 2, size + 4, 4, TRIANGLE_GREEN);
      break;
   case GLYPH_L1R1:
      akeno_canvas_round_rect(f, x - 6, y, 80, size + 4, 10, TEXT_DIM);
      akeno_canvas_text(f, x + 2, y + 10, "L1R1", 2, BG_TOP);
      break;
   }
}

/* A footer hint: glyph and label. Returns the x after it. */
static int
hint(akeno_frame *f, int x, glyph g, const char *label)
{
   draw_glyph(f, x, FOOTER_Y + 14, 28, g);
   const int text_x = x + (g == GLYPH_L1R1 ? 90 : 44);
   const int end = akeno_canvas_text_ex(f, text_x, FOOTER_Y + 18, label, 3, TEXT, SHADOW, 0);
   return end + 48;
}

/* ------------------------------------------------------------- helpers */

static uint32_t
status_color(akeno_row_status s)
{
   switch (s) {
   case AKENO_ROW_OK: return OK_GREEN;
   case AKENO_ROW_WARN: return WARN_AMBER;
   case AKENO_ROW_FAIL: return FAIL_RED;
   case AKENO_ROW_PENDING: return TEXT_DIM;
   case AKENO_ROW_INFO: break;
   }
   return ACCENT;
}

static uint32_t
evidence_color(akeno_evidence e)
{
   switch (e) {
   case AKENO_EV_HARDWARE_VERIFIED: return OK_GREEN;
   case AKENO_EV_HOST_VERIFIED:
   case AKENO_EV_CROSS_BUILD_VERIFIED: return ACCENT;
   case AKENO_EV_IN_PROGRESS: return WARN_AMBER;
   case AKENO_EV_FAILED:
   case AKENO_EV_BLOCKED: return FAIL_RED;
   case AKENO_EV_NOT_STARTED: break;
   }
   return TEXT_DIM;
}

/* Text wrapped at word boundaries to width; returns the y below the last line. */
static int
wrapped(akeno_frame *f, int x, int y, const char *text, int scale, uint32_t color, int width)
{
   const int per_line = width / (8 * scale);
   const int line_height = 8 * scale + scale * 4;
   char line[256];
   const char *at = text;
   while (*at) {
      int take = (int)strlen(at);
      if (take > per_line) {
         take = per_line;
         while (take > 0 && at[take] != ' ')
            take--;
         if (take == 0)
            take = per_line;
      }
      if (take >= (int)sizeof(line))
         take = (int)sizeof(line) - 1;
      memcpy(line, at, (size_t)take);
      line[take] = '\0';
      akeno_canvas_text_ex(f, x, y, line, scale, color, SHADOW, 0);
      y += line_height;
      at += take;
      while (*at == ' ')
         at++;
   }
   return y;
}

static void
chip(akeno_frame *f, int x, int y, const char *label, const char *value, uint32_t color)
{
   akeno_canvas_round_rect(f, x, y, 360, 96, 14, PANEL_SOFT);
   akeno_canvas_fill(f, x + 18, y + 22, 10, 52, color);
   akeno_canvas_text_ex(f, x + 44, y + 20, label, 2, TEXT_DIM, 0, 300);
   akeno_canvas_text_ex(f, x + 44, y + 50, value, 3, TEXT, SHADOW, 300);
}

/* ------------------------------------------------------------- chrome */

static void
draw_background(akeno_frame *f)
{
   akeno_canvas_gradient(f, 0, 0, (int)f->width, (int)f->height, BG_TOP, BG_BOTTOM);
   /* A soft band of light behind the panels. */
   for (int i = 0; i < 12; i++)
      akeno_canvas_blend(f, 0, 640 + i * 18, (int)f->width, 18, AKENO_ARGB(4 + i, 0x2e, 0x7d, 0xff));
}

static void
draw_header(akeno_frame *f, const akeno_view *v)
{
   akeno_canvas_fill(f, 72, 40, 10, 64, ACCENT);
   akeno_canvas_text_ex(f, 104, 40, "AKENOBLOX", 5, TEXT, SHADOW, 0);
   akeno_canvas_text_ex(f, 108, 92, "PS5  -  experimental native runtime", 2, TEXT_DIM, 0, 0);
   char right[96];
   snprintf(right, sizeof(right), "v%s  %s", AKENO_VERSION, AKENO_COMMIT);
   akeno_canvas_text_ex(f, (int)f->width - 72 - akeno_text_width(right, 2), 48, right, 2, TEXT_DIM,
                        0, 0);
   const bool pad = v->pad != NULL && v->pad->connected;
   const char *pad_text = pad ? "Controller connected" : "No controller";
   akeno_canvas_text_ex(f, (int)f->width - 72 - akeno_text_width(pad_text, 2), 80, pad_text, 2,
                        pad ? OK_GREEN : WARN_AMBER, 0, 0);
}

static void
draw_nav(akeno_frame *f, const akeno_menu *m)
{
   for (int i = 0; i < AKENO_HOME_COUNT; i++) {
      const int y = NAV_Y + i * (NAV_H + NAV_GAP);
      const bool selected = i == m->home_index;
      const bool home = m->screen == AKENO_SCREEN_HOME;
      if (selected && home) {
         akeno_canvas_round_rect(f, NAV_X - 6, y - 6, NAV_W + 12, NAV_H + 12, 22, ACCENT_GLOW);
         akeno_canvas_round_rect(f, NAV_X, y, NAV_W, NAV_H, 18, ACCENT);
      } else if (selected) {
         akeno_canvas_round_rect(f, NAV_X, y, NAV_W, NAV_H, 18, AKENO_ARGB(140, 0x2e, 0x7d, 0xff));
      } else {
         akeno_canvas_round_rect(f, NAV_X, y, NAV_W, NAV_H, 18, PANEL_SOFT);
      }
      akeno_canvas_text_ex(f, NAV_X + 32, y + 28, akeno_home_label(i), 3,
                           selected ? TEXT : TEXT_DIM, SHADOW, NAV_W - 48);
   }
}

static void
draw_panel(akeno_frame *f, const char *title)
{
   akeno_canvas_round_rect(f, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 26, PANEL);
   akeno_canvas_outline(f, PANEL_X + 26, PANEL_Y, PANEL_W - 52, 2, 2, AKENO_ARGB(90, 0x2e, 0x7d, 0xff));
   akeno_canvas_text_ex(f, BODY_X, PANEL_Y + 40, title, 4, TEXT, SHADOW, BODY_W);
}

/* ------------------------------------------------------------- screens */

static void
screen_home(akeno_frame *f, const akeno_view *v)
{
   draw_panel(f, "Welcome");
   int y = PANEL_Y + 112;
   y = wrapped(f, BODY_X, y,
               "AkenoBlox is an experimental homebrew runtime that aims to run Roblox's own "
               "Android x86-64 engine locally on a jailbroken PS5: no PSN, no streaming, no PC.",
               3, TEXT, BODY_W);
   y += 12;
   y = wrapped(f, BODY_X, y,
               "It does not run Roblox yet. This build is the launcher and the first layers of the "
               "runtime. Runtime Status shows what is done and what is left.",
               3, TEXT_DIM, BODY_W);
   const akeno_stage *next = akeno_runtime_next_to_build();
   y += 20;
   akeno_canvas_text_ex(f, BODY_X, y, "Next to build", 2, TEXT_DIM, 0, 0);
   akeno_canvas_text_ex(f, BODY_X, y + 28, next ? next->name : "nothing", 3, WARN_AMBER, SHADOW,
                        BODY_W);
   char waiting[96];
   snprintf(waiting, sizeof(waiting), "%zu layers built and awaiting a console run",
            akeno_runtime_awaiting_hardware());
   akeno_canvas_text_ex(f, BODY_X, y + 72, waiting, 2, TEXT_DIM, 0, BODY_W);

   const int chips_y = PANEL_Y + PANEL_H - 140;
   chip(f, BODY_X, chips_y, "Loader self-test",
        !v->probes_ran ? "Not run" : v->selftest_passed ? "Passed" : "Failed",
        !v->probes_ran ? TEXT_DIM : v->selftest_passed ? OK_GREEN : FAIL_RED);
   const bool installed = v->package != NULL && v->package->found;
   chip(f, BODY_X + 384, chips_y, "Roblox package",
        v->package == NULL || !v->package->searched ? "Not checked"
        : installed                                 ? "Found"
                                                    : "Not installed",
        installed ? OK_GREEN : TEXT_DIM);
   const bool pad = v->pad != NULL && v->pad->connected;
   chip(f, BODY_X + 768, chips_y, "Controller", pad ? "Connected" : "None", pad ? OK_GREEN : WARN_AMBER);
}

static void
screen_start(akeno_frame *f, const akeno_view *v)
{
   draw_panel(f, "Start Roblox");
   int y = PANEL_Y + 112;
   akeno_canvas_text_ex(f, BODY_X, y, "Roblox cannot start yet.", 3, FAIL_RED, SHADOW, BODY_W);
   y += 48;
   const akeno_stage *blocker = akeno_runtime_next_to_build();
   if (blocker != NULL) {
      char line[256];
      snprintf(line, sizeof(line), "Blocked at: %s (%s).", blocker->name,
               akeno_evidence_name(blocker->evidence));
      y = wrapped(f, BODY_X, y, line, 3, TEXT, BODY_W);
      y = wrapped(f, BODY_X, y + 4, blocker->note, 2, TEXT_DIM, BODY_W);
   }
   char waiting[128];
   snprintf(waiting, sizeof(waiting),
            "%zu earlier layers are built but not yet proven on a console; Runtime Status lists them.",
            akeno_runtime_awaiting_hardware());
   y = wrapped(f, BODY_X, y + 4, waiting, 2, TEXT_DIM, BODY_W) + 24;
   akeno_canvas_text_ex(f, BODY_X, y, "Roblox package", 3, ACCENT, SHADOW, 0);
   y += 44;
   const akeno_package_check *p = v->package;
   char line[256];
   if (p == NULL || !p->searched) {
      y = wrapped(f, BODY_X, y, "Not checked yet.", 2, TEXT_DIM, BODY_W);
   } else if (!p->found) {
      snprintf(line, sizeof(line), "Not installed. Expected at %.200s", p->path);
      y = wrapped(f, BODY_X, y, line, 2, TEXT, BODY_W);
      y = wrapped(f, BODY_X, y + 8,
                  "AkenoBlox never downloads or ships Roblox. Copy the lib/x86_64/libroblox.so "
                  "from your own copy of Roblox's Android app there (docs/INSTALLATION.md).",
                  2, TEXT_DIM, BODY_W);
   } else {
      snprintf(line, sizeof(line), "%s, %llu MiB", p->valid_elf ? "Valid ELF" : "Not usable",
               (unsigned long long)(p->size >> 20));
      y = wrapped(f, BODY_X, y, line, 2, p->valid_elf ? OK_GREEN : FAIL_RED, BODY_W);
      if (p->valid_elf) {
         snprintf(line, sizeof(line), "Machine %s, Android API %u, NDK %s", p->x86_64 ? "x86-64" : "?",
                  p->android_api, p->ndk[0] ? p->ndk : "unknown");
         y = wrapped(f, BODY_X, y, line, 2, TEXT, BODY_W);
         snprintf(line, sizeof(line), "%ld imports, %ld answered by this build; %u libraries needed",
                  p->import_count, p->imports_answered, p->needed_count);
         y = wrapped(f, BODY_X, y, line, 2, TEXT, BODY_W);
         snprintf(line, sizeof(line), "Relocations: %s%s; TLS: %s; %u segments, alignment 0x%llx",
                  p->aps2 ? "APS2 " : "", p->relr ? "RELR" : "", p->has_tls ? "native" : "emulated",
                  p->load_segments, (unsigned long long)p->max_align);
         y = wrapped(f, BODY_X, y, line, 2, TEXT, BODY_W);
      }
      if (p->problem[0])
         y = wrapped(f, BODY_X, y, p->problem, 2, WARN_AMBER, BODY_W);
   }
   (void)y;
}

static void
screen_runtime(akeno_frame *f, const akeno_view *v, int *list_rows, int *visible_rows)
{
   draw_panel(f, "Runtime Status");
   const akeno_stage *stages;
   const size_t count = akeno_runtime_stages(&stages);
   const int row_h = 82, top = PANEL_Y + 112;
   const int fit = (PANEL_H - 140) / row_h;
   *list_rows = (int)count;
   *visible_rows = fit;
   const int first = v->menu->scroll;
   for (int i = 0; i < fit && first + i < (int)count; i++) {
      const akeno_stage *s = &stages[first + i];
      const int y = top + i * row_h;
      const uint32_t color = evidence_color(s->evidence);
      akeno_canvas_fill(f, BODY_X, y + 6, 8, row_h - 18, color);
      akeno_canvas_text_ex(f, BODY_X + 28, y + 6, s->name, 3, TEXT, SHADOW, BODY_W - 420);
      const char *ev = akeno_evidence_name(s->evidence);
      akeno_canvas_text_ex(f, BODY_X + BODY_W - akeno_text_width(ev, 2), y + 10, ev, 2, color, 0, 0);
      akeno_canvas_text_ex(f, BODY_X + 28, y + 40, s->note, 2, TEXT_DIM, 0, BODY_W - 28);
   }
}

static void
screen_diagnostics(akeno_frame *f, const akeno_view *v, int *list_rows, int *visible_rows)
{
   draw_panel(f, "Diagnostics");
   const akeno_report *r = v->report;
   const int row_h = 36, top = PANEL_Y + 112;
   int fit = (PANEL_H - 140) / row_h;
   int y = top;
   if (!v->probes_ran) {
      y = wrapped(f, BODY_X, y,
                  "Press Cross to run the hardware probes: CPUID, the %fs:0x28 word Android code "
                  "reads, and the ELF loader self-test. Each is logged before it runs.",
                  2, WARN_AMBER, BODY_W) + 12;
      fit -= (y - top) / row_h + 1;
   }
   const int count = r ? r->count : 0;
   *list_rows = count;
   *visible_rows = fit > 1 ? fit : 1;
   const char *section = "";
   for (int i = v->menu->scroll; r && i < count && y + row_h <= PANEL_Y + PANEL_H - 24; i++) {
      const akeno_report_row *row = &r->rows[i];
      if (strcmp(row->section, section) != 0) {
         section = row->section;
         if (i != v->menu->scroll)
            y += 8;
         akeno_canvas_text_ex(f, BODY_X, y, row->section, 2, ACCENT, 0, 0);
         y += row_h;
         if (y + row_h > PANEL_Y + PANEL_H - 24)
            break;
      }
      akeno_canvas_round_rect(f, BODY_X, y + 4, 14, 14, 7, status_color(row->status));
      akeno_canvas_text_ex(f, BODY_X + 30, y, row->label, 2, TEXT, 0, 470);
      akeno_canvas_text_ex(f, BODY_X + 520, y, row->value, 2, TEXT_DIM, 0, BODY_W - 520);
      y += row_h;
   }
}

static void
screen_settings(akeno_frame *f, const akeno_view *v)
{
   const akeno_menu *m = v->menu;
   draw_panel(f, akeno_screen_title(m->screen));
   const int count = akeno_menu_setting_count(m->screen);
   int y = PANEL_Y + 120;
   for (int i = 0; i < count; i++) {
      char label[AKENO_SETTING_LABEL_BYTES], value[AKENO_SETTING_VALUE_BYTES];
      akeno_menu_setting_text(m, m->screen, i, label, value);
      const bool selected = i == m->item_index;
      akeno_canvas_round_rect(f, BODY_X - 16, y - 14, BODY_W + 32, 64, 14,
                              selected ? AKENO_ARGB(200, 0x2e, 0x7d, 0xff) : PANEL_SOFT);
      akeno_canvas_text_ex(f, BODY_X + 8, y + 4, label, 3, TEXT, SHADOW, 560);
      char shown[48];
      snprintf(shown, sizeof(shown), selected ? "<  %s  >" : "%s", value);
      akeno_canvas_text_ex(f, BODY_X + BODY_W - 8 - akeno_text_width(shown, 3), y + 4, shown, 3,
                           selected ? TEXT : TEXT_DIM, SHADOW, 0);
      y += 80;
   }
   y += 12;
   if (m->screen == AKENO_SCREEN_GRAPHICS) {
      wrapped(f, BODY_X, y,
              "These apply to the Roblox engine once it runs; targets, not promises. The launcher "
              "itself always draws at 1920x1080.",
              2, TEXT_DIM, BODY_W);
   } else if (v->mapping != NULL) {
      akeno_canvas_text_ex(f, BODY_X, y, "Mapping (as the Android engine will see it)", 2, ACCENT, 0, 0);
      y += 32;
      static const unsigned shown_buttons[] = {4, 5, 6, 7, 10, 11, 14, 15};
      for (size_t i = 0; i < sizeof(shown_buttons) / sizeof(shown_buttons[0]); i++) {
         char line[64];
         snprintf(line, sizeof(line), "%-9s %s", akeno_button_name(shown_buttons[i]),
                  akeno_keycode_name(v->mapping->keycode[shown_buttons[i]]));
         akeno_canvas_text_ex(f, BODY_X + (int)(i % 2) * 540, y + (int)(i / 2) * 30, line, 2,
                              TEXT_DIM, 0, 520);
      }
   }
}

static void
stick(akeno_frame *f, int cx, int cy, float x, float y, const char *label)
{
   akeno_canvas_round_rect(f, cx - 80, cy - 80, 160, 160, 80, PANEL_SOFT);
   akeno_canvas_round_rect(f, cx - 4, cy - 4, 8, 8, 4, TEXT_DIM);
   const int dx = (int)(x * 64.0f), dy = (int)(y * 64.0f);
   akeno_canvas_round_rect(f, cx + dx - 14, cy + dy - 14, 28, 28, 14, ACCENT);
   akeno_canvas_text_ex(f, cx - akeno_text_width(label, 2) / 2, cy + 96, label, 2, TEXT_DIM, 0, 0);
}

static void
trigger(akeno_frame *f, int x, int y, float value, const char *label)
{
   akeno_canvas_round_rect(f, x, y, 40, 160, 10, PANEL_SOFT);
   const int fill = (int)(value * 152.0f);
   akeno_canvas_round_rect(f, x + 4, y + 156 - fill, 32, fill, 8, ACCENT);
   akeno_canvas_text_ex(f, x + 20 - akeno_text_width(label, 2) / 2, y + 176, label, 2, TEXT_DIM, 0, 0);
}

static void
screen_input_test(akeno_frame *f, const akeno_view *v)
{
   draw_panel(f, "Input Test");
   const akeno_pad_state rest = {0};
   const akeno_pad_state *p = v->pad != NULL ? v->pad : &rest;
   if (!p->connected)
      akeno_canvas_text_ex(f, BODY_X, PANEL_Y + 112, "No controller.", 3, WARN_AMBER, SHADOW, 0);
   /* Buttons as a grid of tiles, lit while held. */
   for (unsigned b = 0; b < AKENO_BTN_COUNT; b++) {
      const int col = (int)(b % 4), row = (int)(b / 4);
      const int x = BODY_X + col * 200, y = PANEL_Y + 168 + row * 72;
      const bool down = (p->buttons & (1u << b)) != 0;
      akeno_canvas_round_rect(f, x, y, 184, 56, 12, down ? ACCENT : PANEL_SOFT);
      akeno_canvas_text_ex(f, x + 16, y + 20, akeno_button_name(b), 2, down ? TEXT : TEXT_DIM, 0, 160);
   }
   stick(f, BODY_X + 920, PANEL_Y + 260, p->left_x, p->left_y, "left stick");
   trigger(f, BODY_X + 1040, PANEL_Y + 180, p->l2, "L2");
   trigger(f, BODY_X + 1100, PANEL_Y + 180, p->r2, "R2");
   stick(f, BODY_X + 920, PANEL_Y + 520, p->right_x, p->right_y, "right stick");

   int y = PANEL_Y + 480;
   akeno_canvas_text_ex(f, BODY_X, y, "As the Android engine would receive it", 2, ACCENT, 0, 0);
   y += 34;
   char keys[256] = "";
   size_t used = 0;
   if (v->gamepad != NULL)
      for (size_t i = 0; i < v->gamepad->key_count && used < sizeof(keys) - 24; i++)
         used += (size_t)snprintf(keys + used, sizeof(keys) - used, "%s ",
                                  akeno_keycode_name(v->gamepad->keys_down[i]));
   akeno_canvas_text_ex(f, BODY_X, y, "Keys down:", 2, TEXT_DIM, 0, 0);
   wrapped(f, BODY_X + 180, y, keys[0] ? keys : "none", 2, TEXT, 600);
   y += 64;
   if (v->gamepad != NULL) {
      char axes[160];
      const akeno_android_gamepad *g = v->gamepad;
      snprintf(axes, sizeof(axes), "X %+.2f  Y %+.2f  Z %+.2f  RZ %+.2f", (double)g->axes[0].value,
               (double)g->axes[1].value, (double)g->axes[2].value, (double)g->axes[3].value);
      akeno_canvas_text_ex(f, BODY_X, y, axes, 2, TEXT, 0, 760);
      snprintf(axes, sizeof(axes), "LTRIGGER %.2f  RTRIGGER %.2f  HAT %+.0f %+.0f",
               (double)g->axes[6].value, (double)g->axes[7].value, (double)g->axes[4].value,
               (double)g->axes[5].value);
      akeno_canvas_text_ex(f, BODY_X, y + 30, axes, 2, TEXT, 0, 760);
   }
   const int held = v->menu->input_test_circle_frames;
   if (held > 0)
      akeno_canvas_fill(f, BODY_X, PANEL_Y + PANEL_H - 40, held * (BODY_W / 60), 8, CIRCLE_RED);
}

static void
screen_about(akeno_frame *f, const akeno_view *v, int *list_rows, int *visible_rows)
{
   draw_panel(f, "About");
   static const char *const lines[] = {
      AKENO_VERSION_STRING,
      "Licence: GPL-3.0-or-later. Source: github.com/YTDxDAkeno/AKENOBLOX",
      "",
      "Experimental software. It may crash or hang; keep your console's data backed up.",
      "Roblox does not support third-party clients and bans accounts for using them. Never sign "
      "in with an account you care about.",
      "",
      "Not affiliated with, endorsed by or sponsored by Roblox Corporation or Sony Interactive "
      "Entertainment. Roblox is a trademark of Roblox Corporation. PlayStation and PS5 are "
      "trademarks of Sony Interactive Entertainment Inc.",
      "",
      "Built on: Cordial (GPL-3.0-or-later), the reference Android runtime; PS5_VulkanTemplate "
      "(MIT); PS5_PayloadSDK platform layer and PS5_Vulkan tools (GPL-3.0); ps5-payload-dev SDK "
      "(GPL-3.0); font8x8 (public domain). Full notices: NOTICE.md.",
   };
   const int count = (int)(sizeof(lines) / sizeof(lines[0]));
   *list_rows = count;
   *visible_rows = count;
   int y = PANEL_Y + 112;
   for (int i = v->menu->scroll; i < count; i++)
      y = lines[i][0] ? wrapped(f, BODY_X, y, lines[i], 2, i == 0 ? TEXT : TEXT_DIM, BODY_W) : y + 20;
}

static void
screen_exit(akeno_frame *f)
{
   draw_panel(f, "Leave AkenoBlox?");
   wrapped(f, BODY_X, PANEL_Y + 120,
           "Settings are saved as you change them. Cross returns to the PS5 home screen.", 3, TEXT,
           BODY_W);
}

static void
draw_footer(akeno_frame *f, const akeno_view *v)
{
   const akeno_menu *m = v->menu;
   int x = 72;
   switch (m->screen) {
   case AKENO_SCREEN_HOME:
      x = hint(f, x, GLYPH_CROSS, "Select");
      x = hint(f, x, GLYPH_CIRCLE, "Exit");
      break;
   case AKENO_SCREEN_START:
      x = hint(f, x, GLYPH_CROSS, "Check again");
      x = hint(f, x, GLYPH_CIRCLE, "Back");
      break;
   case AKENO_SCREEN_DIAGNOSTICS:
      x = hint(f, x, GLYPH_CROSS, "Run probes");
      x = hint(f, x, GLYPH_SQUARE, "Save report");
      x = hint(f, x, GLYPH_L1R1, "Page");
      x = hint(f, x, GLYPH_CIRCLE, "Back");
      break;
   case AKENO_SCREEN_CONTROLLER:
   case AKENO_SCREEN_GRAPHICS:
      x = hint(f, x, GLYPH_CROSS, "Change");
      x = hint(f, x, GLYPH_CIRCLE, "Save and back");
      break;
   case AKENO_SCREEN_INPUT_TEST: x = hint(f, x, GLYPH_CIRCLE, "Hold to go back"); break;
   case AKENO_SCREEN_EXIT:
      x = hint(f, x, GLYPH_CROSS, "Exit");
      x = hint(f, x, GLYPH_CIRCLE, "Stay");
      break;
   default: x = hint(f, x, GLYPH_CIRCLE, "Back"); break;
   }
   (void)x;
   if (v->toast != NULL && v->toast[0]) {
      const int w = akeno_text_width(v->toast, 2) + 48;
      akeno_canvas_round_rect(f, (int)f->width - 72 - w, FOOTER_Y + 4, w, 52, 14,
                              AKENO_ARGB(230, 0x1a, 0x2a, 0x4d));
      akeno_canvas_text_ex(f, (int)f->width - 72 - w + 24, FOOTER_Y + 22, v->toast, 2, TEXT, 0, 0);
   }
}

void
akeno_screens_draw(akeno_frame *f, const akeno_view *v, int *list_rows, int *visible_rows)
{
   int rows = 0, visible = 1;
   draw_background(f);
   draw_header(f, v);
   draw_nav(f, v->menu);
   switch (v->menu->screen) {
   case AKENO_SCREEN_HOME: screen_home(f, v); break;
   case AKENO_SCREEN_START: screen_start(f, v); break;
   case AKENO_SCREEN_RUNTIME: screen_runtime(f, v, &rows, &visible); break;
   case AKENO_SCREEN_DIAGNOSTICS: screen_diagnostics(f, v, &rows, &visible); break;
   case AKENO_SCREEN_CONTROLLER:
   case AKENO_SCREEN_GRAPHICS: screen_settings(f, v); break;
   case AKENO_SCREEN_INPUT_TEST: screen_input_test(f, v); break;
   case AKENO_SCREEN_ABOUT: screen_about(f, v, &rows, &visible); break;
   case AKENO_SCREEN_EXIT: screen_exit(f); break;
   case AKENO_SCREEN_COUNT: break;
   }
   draw_footer(f, v);
   if (list_rows != NULL)
      *list_rows = rows;
   if (visible_rows != NULL)
      *visible_rows = visible;
}
