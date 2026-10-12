/*
 * AkenoBlox PS5 - settings: what the user chose, validated and persisted.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A flat key=value file (<storage>/settings.ini). Unknown keys are kept out
 * rather than rejected, out-of-range values are clamped and reported, and a
 * write goes to a temporary file renamed over the old one, so a crash
 * mid-save leaves the previous settings, not half of the new ones.
 */
#ifndef AKENO_SETTINGS_H
#define AKENO_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum akeno_resolution {
   AKENO_RES_1080P = 0, /* 1920 x 1080 */
   AKENO_RES_1440P,     /* 2560 x 1440 */
   AKENO_RES_2160P,     /* 3840 x 2160 */
   AKENO_RES_COUNT
} akeno_resolution;

typedef enum akeno_touchpad_mode {
   AKENO_TOUCHPAD_CURSOR = 0,
   AKENO_TOUCHPAD_BUTTON,
   AKENO_TOUCHPAD_OFF,
   AKENO_TOUCHPAD_COUNT
} akeno_touchpad_mode;

typedef struct akeno_settings {
   akeno_resolution resolution;
   int frame_rate;       /* 30, 60 or 120: a target, not a promise */
   bool vsync;
   int graphics_quality; /* 0 = let the engine decide, 1..10 */
   int audio_volume;     /* 0..100 */
   int stick_deadzone;   /* percent, 0..40 */
   int camera_sensitivity; /* percent, 10..300 */
   bool invert_camera_y;
   akeno_touchpad_mode touchpad;
   bool verbose_log;
} akeno_settings;

void akeno_settings_defaults(akeno_settings *settings);
/* Field by field: the struct has padding, so memcmp would compare garbage. */
bool akeno_settings_equal(const akeno_settings *a, const akeno_settings *b);

/* Applies one "key=value" line. Returns false for a malformed line or an
 * unknown key (the setting is left alone); *clamped is set when the value was
 * out of range and was clamped. */
bool akeno_settings_apply_line(akeno_settings *settings, const char *line, bool *clamped);

/* Parses a whole file's text; returns how many lines were ignored. */
int akeno_settings_parse(akeno_settings *settings, const char *text);

/* Writes the settings as text; returns the length, or -1 when it did not fit. */
int akeno_settings_format(const akeno_settings *settings, char *out, size_t size);

/* <storage>/settings.ini. load returns false when the file is missing or
 * unreadable (defaults stay); save returns false when it could not write. */
bool akeno_settings_load(akeno_settings *settings, const char *storage_root);
bool akeno_settings_save(const akeno_settings *settings, const char *storage_root);

/* Display names for the UI. */
const char *akeno_resolution_name(akeno_resolution resolution);
void akeno_resolution_size(akeno_resolution resolution, unsigned *width, unsigned *height);
const char *akeno_touchpad_name(akeno_touchpad_mode mode);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_SETTINGS_H */
