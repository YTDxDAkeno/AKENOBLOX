/*
 * AkenoBlox PS5 - drawing the launcher's screens.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A pure function of the menu state and what the app has measured: the same
 * view draws the same pixels, which tests check by hash.
 */
#ifndef AKENO_SCREENS_H
#define AKENO_SCREENS_H

#include <stdbool.h>
#include <stdint.h>

#include "app/akeno_menu.h"
#include "diagnostics/akeno_report.h"
#include "input/akeno_input.h"
#include "runtime/akeno_runtime.h"
#include "ui/akeno_canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The launcher always draws at this size; the resolution setting is the
 * engine's. */
enum { AKENO_UI_WIDTH = 1920, AKENO_UI_HEIGHT = 1080 };

typedef struct akeno_view {
   const akeno_menu *menu;
   const akeno_platform_info *platform;
   const akeno_report *report;          /* the Diagnostics rows */
   bool probes_ran;
   bool selftest_passed;
   const akeno_package_check *package;
   const akeno_pad_state *pad;
   const akeno_android_gamepad *gamepad; /* the pad as the engine would see it */
   const akeno_gamepad_mapping *mapping;
   uint64_t now_us;
   const char *toast;                    /* a one-line notice, or NULL */
   const char *storage_root;
} akeno_view;

/* Draws the current screen. *list_rows and *visible_rows receive the length
 * of the screen's scrollable list and how many rows fit, for the menu. */
void akeno_screens_draw(akeno_frame *frame, const akeno_view *view, int *list_rows,
                        int *visible_rows);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_SCREENS_H */
