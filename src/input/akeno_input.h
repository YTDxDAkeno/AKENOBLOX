/*
 * AkenoBlox PS5 - input: menu navigation, and the DualSense as an Android
 * gamepad.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Two jobs. The launcher needs edges and key repeat from the pad. The runtime
 * will hand Roblox's Android engine what Android would: key events with
 * KeyEvent keycodes and motion events with MotionEvent axes, from a device
 * with SOURCE_GAMEPAD | SOURCE_JOYSTICK, which is how GameActivity reports a
 * controller (AGDK GameActivity, Apache-2.0; Cordial android/gamepad.rs). The
 * mapping from DualSense buttons to keycodes is data, configurable, and
 * validated, never hardcoded per experience.
 */
#ifndef AKENO_INPUT_H
#define AKENO_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "akeno_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------- menu input */

typedef struct akeno_menu_input {
   uint32_t held;     /* AKENO_BTN_* */
   uint32_t pressed;  /* went down this poll, or repeated */
   uint32_t released; /* went up this poll */
} akeno_menu_input;

typedef struct akeno_input_tracker {
   uint32_t previous;
   uint64_t repeat_due_us[AKENO_BTN_COUNT];
   bool stick_engaged[4]; /* the left stick held past the threshold: up, down, left, right */
} akeno_input_tracker;

enum {
   AKENO_REPEAT_DELAY_US = 350000,
   AKENO_REPEAT_INTERVAL_US = 90000,
};

void akeno_input_tracker_reset(akeno_input_tracker *tracker);
/* Folds the left stick into the D-pad (with hysteresis), then reports edges;
 * the D-pad repeats while held. */
akeno_menu_input akeno_input_track(akeno_input_tracker *tracker, const akeno_pad_state *pad,
                                   uint64_t now_us);

/* ------------------------------------------------ the Android gamepad */

/* android.view.KeyEvent keycodes (public SDK constants). */
enum {
   AKENO_KEYCODE_UNKNOWN = 0,
   AKENO_KEYCODE_DPAD_UP = 19,
   AKENO_KEYCODE_DPAD_DOWN = 20,
   AKENO_KEYCODE_DPAD_LEFT = 21,
   AKENO_KEYCODE_DPAD_RIGHT = 22,
   AKENO_KEYCODE_BUTTON_A = 96,
   AKENO_KEYCODE_BUTTON_B = 97,
   AKENO_KEYCODE_BUTTON_X = 99,
   AKENO_KEYCODE_BUTTON_Y = 100,
   AKENO_KEYCODE_BUTTON_L1 = 102,
   AKENO_KEYCODE_BUTTON_R1 = 103,
   AKENO_KEYCODE_BUTTON_L2 = 104,
   AKENO_KEYCODE_BUTTON_R2 = 105,
   AKENO_KEYCODE_BUTTON_THUMBL = 106,
   AKENO_KEYCODE_BUTTON_THUMBR = 107,
   AKENO_KEYCODE_BUTTON_START = 108,
   AKENO_KEYCODE_BUTTON_SELECT = 109,
};

/* android.view.MotionEvent axes (public SDK constants). */
enum {
   AKENO_AXIS_X = 0,
   AKENO_AXIS_Y = 1,
   AKENO_AXIS_Z = 11,
   AKENO_AXIS_RZ = 14,
   AKENO_AXIS_HAT_X = 15,
   AKENO_AXIS_HAT_Y = 16,
   AKENO_AXIS_LTRIGGER = 17,
   AKENO_AXIS_RTRIGGER = 18,
   AKENO_AXIS_SLOTS = 8
};

/* Which keycode each DualSense button sends (AKENO_KEYCODE_UNKNOWN: none). */
typedef struct akeno_gamepad_mapping {
   int keycode[AKENO_BTN_COUNT];
} akeno_gamepad_mapping;

/* The default the project documents (README, Controls): Cross A (jump,
 * confirm), Circle B (back), Square X, Triangle Y, L1/R1, L2/R2 as buttons
 * and triggers, L3/R3 the thumb buttons, Options START (the Roblox menu),
 * the touchpad SELECT, the D-pad the D-pad. */
void akeno_gamepad_mapping_default(akeno_gamepad_mapping *mapping);

/* "button=KEYCODE_NAME" pairs separated by commas, e.g.
 * "cross=BUTTON_A,circle=BUTTON_B,touchpad=NONE". Applied over the current
 * mapping; false (and the mapping unchanged) on any unknown name. */
bool akeno_gamepad_mapping_parse(akeno_gamepad_mapping *mapping, const char *text);
const char *akeno_button_name(unsigned button_index);
const char *akeno_keycode_name(int keycode);

typedef struct akeno_android_axis {
   int axis;
   float value;
} akeno_android_axis;

typedef struct akeno_android_gamepad {
   /* Keycodes held now, sorted ascending, without duplicates. */
   int keys_down[AKENO_BTN_COUNT];
   size_t key_count;
   akeno_android_axis axes[AKENO_AXIS_SLOTS];
   size_t axis_count;
} akeno_android_gamepad;

/* The pad as Android would report it: sticks with a radial dead zone
 * (percent, 0..40) rescaled to keep full range, triggers 0..1, the D-pad
 * also as the hat axes. */
void akeno_android_gamepad_from_pad(const akeno_pad_state *pad, const akeno_gamepad_mapping *mapping,
                                    int deadzone_percent, akeno_android_gamepad *out);

/* A key edge between two reports, for the event queue. */
typedef struct akeno_key_edge {
   int keycode;
   bool down;
} akeno_key_edge;

/* Writes up to max edges taking previous to current (releases first), returns
 * how many. */
size_t akeno_android_key_edges(const akeno_android_gamepad *previous,
                               const akeno_android_gamepad *current, akeno_key_edge *edges,
                               size_t max);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_INPUT_H */
