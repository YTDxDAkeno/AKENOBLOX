/*
 * AkenoBlox PS5 - the launcher's navigation.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_menu.h"

#include <stdio.h>
#include <string.h>

static const char *const home_labels[AKENO_HOME_COUNT] = {
   "Start Roblox", "Runtime Status", "Diagnostics", "Controller", "Graphics & Audio", "About",
   "Exit",
};

static const akeno_screen home_targets[AKENO_HOME_COUNT] = {
   AKENO_SCREEN_START,      AKENO_SCREEN_RUNTIME,  AKENO_SCREEN_DIAGNOSTICS, AKENO_SCREEN_CONTROLLER,
   AKENO_SCREEN_GRAPHICS,   AKENO_SCREEN_ABOUT,    AKENO_SCREEN_EXIT,
};

static const char *const screen_titles[AKENO_SCREEN_COUNT] = {
   "Home", "Start Roblox", "Runtime Status", "Diagnostics", "Controller", "Input Test",
   "Graphics & Audio", "About", "Exit",
};

const char *
akeno_home_label(int index)
{
   return index >= 0 && index < AKENO_HOME_COUNT ? home_labels[index] : "";
}

const char *
akeno_screen_title(akeno_screen screen)
{
   return (unsigned)screen < AKENO_SCREEN_COUNT ? screen_titles[screen] : "";
}

void
akeno_menu_init(akeno_menu *m, akeno_settings *settings, uint64_t now_us)
{
   memset(m, 0, sizeof(*m));
   m->screen = AKENO_SCREEN_HOME;
   m->settings = settings;
   m->visible_rows = 1;
   m->entered_us = now_us;
}

static void
enter(akeno_menu *m, akeno_screen screen, uint64_t now_us)
{
   m->screen = screen;
   m->item_index = 0;
   m->scroll = 0;
   m->list_rows = 0;
   m->entered_us = now_us;
   m->input_test_circle_frames = 0;
}

/* ------------------------------------------------------------ settings */

/* Each settings screen is a list of these, adjusted with left and right. */
typedef enum setting_id {
   SET_RESOLUTION,
   SET_FRAME_RATE,
   SET_VSYNC,
   SET_QUALITY,
   SET_VOLUME,
   SET_VERBOSE_LOG,
   SET_DEADZONE,
   SET_SENSITIVITY,
   SET_INVERT_Y,
   SET_TOUCHPAD,
   SET_INPUT_TEST, /* an entry, not a value */
} setting_id;

static const setting_id graphics_settings[] = {SET_RESOLUTION, SET_FRAME_RATE, SET_VSYNC,
                                               SET_QUALITY,    SET_VOLUME,     SET_VERBOSE_LOG};
static const setting_id controller_settings[] = {SET_DEADZONE, SET_SENSITIVITY, SET_INVERT_Y,
                                                 SET_TOUCHPAD, SET_INPUT_TEST};

static const setting_id *
settings_of(akeno_screen screen, int *count)
{
   if (screen == AKENO_SCREEN_GRAPHICS) {
      *count = (int)(sizeof(graphics_settings) / sizeof(graphics_settings[0]));
      return graphics_settings;
   }
   if (screen == AKENO_SCREEN_CONTROLLER) {
      *count = (int)(sizeof(controller_settings) / sizeof(controller_settings[0]));
      return controller_settings;
   }
   *count = 0;
   return NULL;
}

int
akeno_menu_setting_count(akeno_screen screen)
{
   int count;
   settings_of(screen, &count);
   return count;
}

static int
step(int value, int delta, int low, int high, int by)
{
   value += delta * by;
   return value < low ? low : value > high ? high : value;
}

/* Changes a setting by delta (-1 left, +1 right, 0 a press of Cross).
 * Returns true when it changed. */
static bool
adjust(akeno_settings *s, setting_id id, int delta)
{
   const akeno_settings before = *s;
   const int toggle = delta == 0 ? 1 : delta;
   switch (id) {
   case SET_RESOLUTION:
      s->resolution = (akeno_resolution)(((int)s->resolution + toggle + AKENO_RES_COUNT) % AKENO_RES_COUNT);
      break;
   case SET_FRAME_RATE: {
      static const int rates[] = {30, 60, 120};
      int i = s->frame_rate == 30 ? 0 : s->frame_rate == 120 ? 2 : 1;
      i = (i + toggle + 3) % 3;
      s->frame_rate = rates[i];
      break;
   }
   case SET_VSYNC: s->vsync = !s->vsync; break;
   case SET_QUALITY: s->graphics_quality = step(s->graphics_quality, delta ? delta : 1, 0, 10, 1); break;
   case SET_VOLUME: s->audio_volume = step(s->audio_volume, delta ? delta : 1, 0, 100, 5); break;
   case SET_VERBOSE_LOG: s->verbose_log = !s->verbose_log; break;
   case SET_DEADZONE: s->stick_deadzone = step(s->stick_deadzone, delta ? delta : 1, 0, 40, 2); break;
   case SET_SENSITIVITY:
      s->camera_sensitivity = step(s->camera_sensitivity, delta ? delta : 1, 10, 300, 10);
      break;
   case SET_INVERT_Y: s->invert_camera_y = !s->invert_camera_y; break;
   case SET_TOUCHPAD:
      s->touchpad = (akeno_touchpad_mode)(((int)s->touchpad + toggle + AKENO_TOUCHPAD_COUNT) %
                                          AKENO_TOUCHPAD_COUNT);
      break;
   case SET_INPUT_TEST: return false;
   }
   return !akeno_settings_equal(&before, s);
}

void
akeno_menu_setting_text(const akeno_menu *m, akeno_screen screen, int index, char *label,
                        char *value)
{
   int count;
   const setting_id *ids = settings_of(screen, &count);
   label[0] = value[0] = '\0';
   if (ids == NULL || index < 0 || index >= count || m->settings == NULL)
      return;
   const akeno_settings *s = m->settings;
   const size_t L = AKENO_SETTING_LABEL_BYTES, V = AKENO_SETTING_VALUE_BYTES;
   switch (ids[index]) {
   case SET_RESOLUTION:
      snprintf(label, L, "Resolution");
      snprintf(value, V, "%s", akeno_resolution_name(s->resolution));
      break;
   case SET_FRAME_RATE:
      snprintf(label, L, "Frame rate target");
      snprintf(value, V, "%d fps", s->frame_rate);
      break;
   case SET_VSYNC:
      snprintf(label, L, "VSync");
      snprintf(value, V, "%s", s->vsync ? "On" : "Off");
      break;
   case SET_QUALITY:
      snprintf(label, L, "Graphics quality");
      if (s->graphics_quality == 0)
         snprintf(value, V, "Automatic");
      else
         snprintf(value, V, "%d", s->graphics_quality);
      break;
   case SET_VOLUME:
      snprintf(label, L, "Audio volume");
      snprintf(value, V, "%d%%", s->audio_volume);
      break;
   case SET_VERBOSE_LOG:
      snprintf(label, L, "Verbose log");
      snprintf(value, V, "%s", s->verbose_log ? "On" : "Off");
      break;
   case SET_DEADZONE:
      snprintf(label, L, "Stick dead zone");
      snprintf(value, V, "%d%%", s->stick_deadzone);
      break;
   case SET_SENSITIVITY:
      snprintf(label, L, "Camera sensitivity");
      snprintf(value, V, "%d%%", s->camera_sensitivity);
      break;
   case SET_INVERT_Y:
      snprintf(label, L, "Invert camera Y");
      snprintf(value, V, "%s", s->invert_camera_y ? "On" : "Off");
      break;
   case SET_TOUCHPAD:
      snprintf(label, L, "Touchpad");
      snprintf(value, V, "%s", akeno_touchpad_name(s->touchpad));
      break;
   case SET_INPUT_TEST:
      snprintf(label, L, "Input test");
      snprintf(value, V, "Open");
      break;
   }
}

/* ------------------------------------------------------------- update */

static int
wrap(int value, int count)
{
   return count <= 0 ? 0 : (value % count + count) % count;
}

static void
scroll_list(akeno_menu *m, const akeno_menu_input *in)
{
   const int page = m->visible_rows > 1 ? m->visible_rows : 1;
   const int last = m->list_rows > page ? m->list_rows - page : 0;
   if (in->pressed & AKENO_BTN_DOWN)
      m->scroll++;
   if (in->pressed & AKENO_BTN_UP)
      m->scroll--;
   if (in->pressed & AKENO_BTN_R1)
      m->scroll += page;
   if (in->pressed & AKENO_BTN_L1)
      m->scroll -= page;
   if (m->scroll > last)
      m->scroll = last;
   if (m->scroll < 0)
      m->scroll = 0;
}

static akeno_command
leave_settings(akeno_menu *m, uint64_t now_us)
{
   const bool dirty = m->settings_dirty;
   m->settings_dirty = false;
   enter(m, AKENO_SCREEN_HOME, now_us);
   return dirty ? AKENO_CMD_SAVE_SETTINGS : AKENO_CMD_NONE;
}

akeno_command
akeno_menu_update(akeno_menu *m, const akeno_menu_input *in, uint64_t now_us)
{
   const uint32_t p = in->pressed;
   switch (m->screen) {
   case AKENO_SCREEN_HOME:
      if (p & AKENO_BTN_DOWN)
         m->home_index = wrap(m->home_index + 1, AKENO_HOME_COUNT);
      if (p & AKENO_BTN_UP)
         m->home_index = wrap(m->home_index - 1, AKENO_HOME_COUNT);
      if (p & AKENO_BTN_CROSS) {
         const akeno_screen target = home_targets[m->home_index];
         enter(m, target, now_us);
         if (target == AKENO_SCREEN_START)
            return AKENO_CMD_CHECK_PACKAGE;
      }
      if (p & AKENO_BTN_CIRCLE)
         m->home_index = AKENO_HOME_EXIT;
      return AKENO_CMD_NONE;

   case AKENO_SCREEN_START:
      if (p & AKENO_BTN_CIRCLE) {
         enter(m, AKENO_SCREEN_HOME, now_us);
         return AKENO_CMD_NONE;
      }
      if (p & AKENO_BTN_CROSS)
         return AKENO_CMD_CHECK_PACKAGE;
      return AKENO_CMD_NONE;

   case AKENO_SCREEN_RUNTIME:
   case AKENO_SCREEN_ABOUT:
      if (p & AKENO_BTN_CIRCLE) {
         enter(m, AKENO_SCREEN_HOME, now_us);
         return AKENO_CMD_NONE;
      }
      scroll_list(m, in);
      return AKENO_CMD_NONE;

   case AKENO_SCREEN_DIAGNOSTICS:
      if (p & AKENO_BTN_CIRCLE) {
         enter(m, AKENO_SCREEN_HOME, now_us);
         return AKENO_CMD_NONE;
      }
      scroll_list(m, in);
      if (p & AKENO_BTN_CROSS)
         return AKENO_CMD_RUN_PROBES;
      if (p & AKENO_BTN_SQUARE)
         return AKENO_CMD_EXPORT_REPORT;
      return AKENO_CMD_NONE;

   case AKENO_SCREEN_CONTROLLER:
   case AKENO_SCREEN_GRAPHICS: {
      int count;
      const setting_id *ids = settings_of(m->screen, &count);
      if (p & AKENO_BTN_CIRCLE)
         return leave_settings(m, now_us);
      if (p & AKENO_BTN_DOWN)
         m->item_index = wrap(m->item_index + 1, count);
      if (p & AKENO_BTN_UP)
         m->item_index = wrap(m->item_index - 1, count);
      const setting_id id = ids[m->item_index];
      if (id == SET_INPUT_TEST) {
         if (p & AKENO_BTN_CROSS) {
            const bool dirty = m->settings_dirty;
            enter(m, AKENO_SCREEN_INPUT_TEST, now_us);
            m->settings_dirty = false;
            return dirty ? AKENO_CMD_SAVE_SETTINGS : AKENO_CMD_NONE;
         }
         return AKENO_CMD_NONE;
      }
      if (m->settings != NULL) {
         int delta = 0;
         bool act = false;
         if (p & AKENO_BTN_LEFT) {
            delta = -1;
            act = true;
         } else if (p & AKENO_BTN_RIGHT) {
            delta = 1;
            act = true;
         } else if (p & AKENO_BTN_CROSS) {
            act = true;
         }
         if (act && adjust(m->settings, id, delta))
            m->settings_dirty = true;
      }
      return AKENO_CMD_NONE;
   }

   case AKENO_SCREEN_INPUT_TEST:
      /* Every button is under test here, Circle included, so leaving takes a
       * deliberate hold: Circle for about a second (60 polls). */
      if (in->held & AKENO_BTN_CIRCLE) {
         if (++m->input_test_circle_frames >= 60) {
            enter(m, AKENO_SCREEN_CONTROLLER, now_us);
            m->item_index = akeno_menu_setting_count(AKENO_SCREEN_CONTROLLER) - 1;
         }
      } else {
         m->input_test_circle_frames = 0;
      }
      return AKENO_CMD_NONE;

   case AKENO_SCREEN_EXIT:
      if (p & AKENO_BTN_CROSS)
         return AKENO_CMD_EXIT;
      if (p & AKENO_BTN_CIRCLE)
         enter(m, AKENO_SCREEN_HOME, now_us);
      return AKENO_CMD_NONE;

   case AKENO_SCREEN_COUNT: break;
   }
   return AKENO_CMD_NONE;
}
