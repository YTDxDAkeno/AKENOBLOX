/*
 * AkenoBlox PS5 - menu tests: whole journeys through the launcher.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_test.h"

#include "app/akeno_menu.h"

static akeno_settings settings;
static akeno_menu menu;
static uint64_t clock_us;

static void
reset(void)
{
   akeno_settings_defaults(&settings);
   clock_us = 0;
   akeno_menu_init(&menu, &settings, clock_us);
   menu.visible_rows = 5;
}

static akeno_command
press(uint32_t buttons)
{
   clock_us += 100000;
   const akeno_menu_input in = {.held = buttons, .pressed = buttons, .released = 0};
   return akeno_menu_update(&menu, &in, clock_us);
}

static akeno_command
hold(uint32_t buttons)
{
   clock_us += 16000;
   const akeno_menu_input in = {.held = buttons, .pressed = 0, .released = 0};
   return akeno_menu_update(&menu, &in, clock_us);
}

static void
test_home_navigation_wraps(void)
{
   reset();
   CHECK_EQ_INT(menu.screen, AKENO_SCREEN_HOME);
   CHECK_EQ_INT(menu.home_index, AKENO_HOME_START);
   press(AKENO_BTN_UP);
   CHECK_EQ_INT(menu.home_index, AKENO_HOME_EXIT);
   press(AKENO_BTN_DOWN);
   CHECK_EQ_INT(menu.home_index, AKENO_HOME_START);
   for (int i = 0; i < AKENO_HOME_COUNT; i++)
      CHECK(akeno_home_label(i)[0] != '\0');
   /* Circle on home moves to Exit rather than leaving. */
   press(AKENO_BTN_CIRCLE);
   CHECK_EQ_INT(menu.home_index, AKENO_HOME_EXIT);
   CHECK_EQ_INT(menu.screen, AKENO_SCREEN_HOME);
}

static void
test_start_checks_the_package(void)
{
   reset();
   CHECK_EQ_INT(press(AKENO_BTN_CROSS), AKENO_CMD_CHECK_PACKAGE);
   CHECK_EQ_INT(menu.screen, AKENO_SCREEN_START);
   CHECK_EQ_INT(press(AKENO_BTN_CROSS), AKENO_CMD_CHECK_PACKAGE);
   CHECK_EQ_INT(press(AKENO_BTN_CIRCLE), AKENO_CMD_NONE);
   CHECK_EQ_INT(menu.screen, AKENO_SCREEN_HOME);
}

static void
test_diagnostics_commands_and_scroll(void)
{
   reset();
   menu.home_index = AKENO_HOME_DIAGNOSTICS;
   press(AKENO_BTN_CROSS);
   REQUIRE(menu.screen == AKENO_SCREEN_DIAGNOSTICS);
   CHECK_EQ_INT(press(AKENO_BTN_CROSS), AKENO_CMD_RUN_PROBES);
   CHECK_EQ_INT(press(AKENO_BTN_SQUARE), AKENO_CMD_EXPORT_REPORT);
   menu.list_rows = 12;
   menu.visible_rows = 5;
   press(AKENO_BTN_DOWN);
   CHECK_EQ_INT(menu.scroll, 1);
   press(AKENO_BTN_R1);
   CHECK_EQ_INT(menu.scroll, 6);
   press(AKENO_BTN_R1); /* clamped to the last page */
   CHECK_EQ_INT(menu.scroll, 7);
   press(AKENO_BTN_L1);
   press(AKENO_BTN_L1);
   CHECK_EQ_INT(menu.scroll, 0);
   press(AKENO_BTN_UP);
   CHECK_EQ_INT(menu.scroll, 0);
}

static void
test_graphics_settings_saved_on_leave(void)
{
   reset();
   menu.home_index = AKENO_HOME_GRAPHICS;
   press(AKENO_BTN_CROSS);
   REQUIRE(menu.screen == AKENO_SCREEN_GRAPHICS);
   char label[AKENO_SETTING_LABEL_BYTES], value[AKENO_SETTING_VALUE_BYTES];
   akeno_menu_setting_text(&menu, AKENO_SCREEN_GRAPHICS, 0, label, value);
   CHECK_EQ_STR(label, "Resolution");
   CHECK_EQ_STR(value, "1920x1080");
   press(AKENO_BTN_RIGHT);
   CHECK_EQ_INT(settings.resolution, AKENO_RES_1440P);
   press(AKENO_BTN_LEFT);
   press(AKENO_BTN_LEFT);
   CHECK_EQ_INT(settings.resolution, AKENO_RES_2160P); /* wraps */
   press(AKENO_BTN_DOWN);
   press(AKENO_BTN_RIGHT);
   CHECK_EQ_INT(settings.frame_rate, 120);
   press(AKENO_BTN_DOWN);
   press(AKENO_BTN_CROSS); /* vsync toggles on Cross */
   CHECK(!settings.vsync);
   press(AKENO_BTN_DOWN);
   for (int i = 0; i < 20; i++)
      press(AKENO_BTN_RIGHT);
   CHECK_EQ_INT(settings.graphics_quality, 10); /* clamped */
   press(AKENO_BTN_DOWN);
   for (int i = 0; i < 30; i++)
      press(AKENO_BTN_LEFT);
   CHECK_EQ_INT(settings.audio_volume, 0);
   CHECK(menu.settings_dirty);
   CHECK_EQ_INT(press(AKENO_BTN_CIRCLE), AKENO_CMD_SAVE_SETTINGS);
   CHECK_EQ_INT(menu.screen, AKENO_SCREEN_HOME);
   CHECK(!menu.settings_dirty);
   /* Leaving without a change saves nothing. */
   menu.home_index = AKENO_HOME_GRAPHICS;
   press(AKENO_BTN_CROSS);
   CHECK_EQ_INT(press(AKENO_BTN_CIRCLE), AKENO_CMD_NONE);
}

static void
test_input_test_needs_a_held_circle(void)
{
   reset();
   menu.home_index = AKENO_HOME_CONTROLLER;
   press(AKENO_BTN_CROSS);
   REQUIRE(menu.screen == AKENO_SCREEN_CONTROLLER);
   const int count = akeno_menu_setting_count(AKENO_SCREEN_CONTROLLER);
   for (int i = 0; i < count - 1; i++)
      press(AKENO_BTN_DOWN);
   char label[AKENO_SETTING_LABEL_BYTES], value[AKENO_SETTING_VALUE_BYTES];
   akeno_menu_setting_text(&menu, AKENO_SCREEN_CONTROLLER, menu.item_index, label, value);
   CHECK_EQ_STR(label, "Input test");
   press(AKENO_BTN_CROSS);
   REQUIRE(menu.screen == AKENO_SCREEN_INPUT_TEST);
   /* A tap of Circle is a button under test, not a way out. */
   press(AKENO_BTN_CIRCLE);
   hold(0);
   CHECK_EQ_INT(menu.screen, AKENO_SCREEN_INPUT_TEST);
   for (int i = 0; i < 59; i++)
      hold(AKENO_BTN_CIRCLE);
   CHECK_EQ_INT(menu.screen, AKENO_SCREEN_INPUT_TEST);
   hold(AKENO_BTN_CIRCLE);
   CHECK_EQ_INT(menu.screen, AKENO_SCREEN_CONTROLLER);
   CHECK_EQ_INT(menu.item_index, count - 1);
}

static void
test_exit_confirmation(void)
{
   reset();
   menu.home_index = AKENO_HOME_EXIT;
   press(AKENO_BTN_CROSS);
   REQUIRE(menu.screen == AKENO_SCREEN_EXIT);
   CHECK_EQ_INT(press(AKENO_BTN_CIRCLE), AKENO_CMD_NONE);
   CHECK_EQ_INT(menu.screen, AKENO_SCREEN_HOME);
   menu.home_index = AKENO_HOME_EXIT;
   press(AKENO_BTN_CROSS);
   CHECK_EQ_INT(press(AKENO_BTN_CROSS), AKENO_CMD_EXIT);
}

static void
test_every_screen_has_a_title(void)
{
   for (int s = 0; s < AKENO_SCREEN_COUNT; s++)
      CHECK(akeno_screen_title((akeno_screen)s)[0] != '\0');
   CHECK_EQ_INT(akeno_menu_setting_count(AKENO_SCREEN_HOME), 0);
}

static const akeno_test_case cases[] = {
   {"home_navigation_wraps", test_home_navigation_wraps},
   {"start_checks_the_package", test_start_checks_the_package},
   {"diagnostics_commands_and_scroll", test_diagnostics_commands_and_scroll},
   {"graphics_settings_saved_on_leave", test_graphics_settings_saved_on_leave},
   {"input_test_needs_a_held_circle", test_input_test_needs_a_held_circle},
   {"exit_confirmation", test_exit_confirmation},
   {"every_screen_has_a_title", test_every_screen_has_a_title},
};

AKENO_TEST_MAIN(cases)
