/*
 * AkenoBlox PS5 - the launcher's navigation: screens, selection and the
 * settings they edit, with no drawing and no platform calls.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The menu turns pad edges into a new state and, when the program must do
 * something (run the hardware probes, save, exit), a command for akeno_app to
 * carry out. Keeping it pure is what lets tests drive whole journeys through
 * it on a host.
 */
#ifndef AKENO_MENU_H
#define AKENO_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "app/akeno_settings.h"
#include "input/akeno_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum akeno_screen {
   AKENO_SCREEN_HOME = 0,
   AKENO_SCREEN_START,       /* Start Roblox: what is installed, and why it cannot start yet */
   AKENO_SCREEN_RUNTIME,     /* the runtime's layers and their evidence */
   AKENO_SCREEN_DIAGNOSTICS,
   AKENO_SCREEN_CONTROLLER,
   AKENO_SCREEN_INPUT_TEST,
   AKENO_SCREEN_GRAPHICS,
   AKENO_SCREEN_ABOUT,
   AKENO_SCREEN_EXIT,        /* confirm leaving */
   AKENO_SCREEN_COUNT
} akeno_screen;

typedef enum akeno_command {
   AKENO_CMD_NONE = 0,
   AKENO_CMD_RUN_PROBES,
   AKENO_CMD_EXPORT_REPORT,
   AKENO_CMD_CHECK_PACKAGE,
   AKENO_CMD_SAVE_SETTINGS,
   AKENO_CMD_EXIT,
} akeno_command;

/* The home screen's entries, in order. */
enum {
   AKENO_HOME_START = 0,
   AKENO_HOME_RUNTIME,
   AKENO_HOME_DIAGNOSTICS,
   AKENO_HOME_CONTROLLER,
   AKENO_HOME_GRAPHICS,
   AKENO_HOME_ABOUT,
   AKENO_HOME_EXIT,
   AKENO_HOME_COUNT
};

typedef struct akeno_menu {
   akeno_screen screen;
   int home_index;
   int item_index;    /* the selected setting on a settings screen */
   int scroll;        /* the first row shown on a list screen */
   int list_rows;     /* rows the current list screen has; the app sets it */
   int visible_rows;  /* rows that fit; the app sets it */
   akeno_settings *settings;
   bool settings_dirty;
   uint64_t entered_us; /* when the current screen was entered, for its animation */
   int input_test_circle_frames; /* Circle held on the input test, to leave it */
} akeno_menu;

const char *akeno_home_label(int index);
const char *akeno_screen_title(akeno_screen screen);

void akeno_menu_init(akeno_menu *menu, akeno_settings *settings, uint64_t now_us);
akeno_command akeno_menu_update(akeno_menu *menu, const akeno_menu_input *in, uint64_t now_us);

/* A settings screen's rows. */
enum { AKENO_SETTING_LABEL_BYTES = 32, AKENO_SETTING_VALUE_BYTES = 32 };
int akeno_menu_setting_count(akeno_screen screen);
void akeno_menu_setting_text(const akeno_menu *menu, akeno_screen screen, int index, char *label,
                             char *value);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_MENU_H */
