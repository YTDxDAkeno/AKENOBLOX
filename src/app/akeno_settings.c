/*
 * AkenoBlox PS5 - settings.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_settings.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *const resolution_names[AKENO_RES_COUNT] = {"1920x1080", "2560x1440",
                                                              "3840x2160"};
static const char *const touchpad_names[AKENO_TOUCHPAD_COUNT] = {"cursor", "button", "off"};

void
akeno_settings_defaults(akeno_settings *s)
{
   *s = (akeno_settings){
      .resolution = AKENO_RES_1080P,
      .frame_rate = 60,
      .vsync = true,
      .graphics_quality = 0,
      .audio_volume = 80,
      .stick_deadzone = 12,
      .camera_sensitivity = 100,
      .invert_camera_y = false,
      .touchpad = AKENO_TOUCHPAD_CURSOR,
      .verbose_log = false,
   };
}

bool
akeno_settings_equal(const akeno_settings *a, const akeno_settings *b)
{
   return a->resolution == b->resolution && a->frame_rate == b->frame_rate &&
          a->vsync == b->vsync && a->graphics_quality == b->graphics_quality &&
          a->audio_volume == b->audio_volume && a->stick_deadzone == b->stick_deadzone &&
          a->camera_sensitivity == b->camera_sensitivity &&
          a->invert_camera_y == b->invert_camera_y && a->touchpad == b->touchpad &&
          a->verbose_log == b->verbose_log;
}

const char *
akeno_resolution_name(akeno_resolution r)
{
   return (unsigned)r < AKENO_RES_COUNT ? resolution_names[r] : "unknown";
}

void
akeno_resolution_size(akeno_resolution r, unsigned *width, unsigned *height)
{
   static const unsigned sizes[AKENO_RES_COUNT][2] = {{1920, 1080}, {2560, 1440}, {3840, 2160}};
   const unsigned i = (unsigned)r < AKENO_RES_COUNT ? (unsigned)r : 0;
   *width = sizes[i][0];
   *height = sizes[i][1];
}

const char *
akeno_touchpad_name(akeno_touchpad_mode mode)
{
   return (unsigned)mode < AKENO_TOUCHPAD_COUNT ? touchpad_names[mode] : "unknown";
}

static void
trim(char *s)
{
   size_t length = strlen(s);
   while (length > 0 && isspace((unsigned char)s[length - 1]))
      s[--length] = '\0';
   size_t start = 0;
   while (isspace((unsigned char)s[start]))
      start++;
   memmove(s, s + start, length - start + 1);
}

static bool
parse_int(const char *text, int *value)
{
   char *end;
   errno = 0;
   const long parsed = strtol(text, &end, 10);
   if (errno != 0 || end == text || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX)
      return false;
   *value = (int)parsed;
   return true;
}

static bool
parse_bool(const char *text, bool *value)
{
   if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0 || strcmp(text, "on") == 0) {
      *value = true;
      return true;
   }
   if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0 || strcmp(text, "off") == 0) {
      *value = false;
      return true;
   }
   return false;
}

static int
clamp(int value, int low, int high, bool *clamped)
{
   if (value < low) {
      *clamped = true;
      return low;
   }
   if (value > high) {
      *clamped = true;
      return high;
   }
   return value;
}

static bool
parse_name(const char *text, const char *const *names, int count, int *value)
{
   for (int i = 0; i < count; i++)
      if (strcmp(text, names[i]) == 0) {
         *value = i;
         return true;
      }
   return false;
}

bool
akeno_settings_apply_line(akeno_settings *s, const char *line, bool *clamped_out)
{
   bool clamped = false;
   if (clamped_out != NULL)
      *clamped_out = false;
   char buffer[256];
   if (line == NULL || strlen(line) >= sizeof(buffer))
      return false;
   snprintf(buffer, sizeof(buffer), "%s", line);
   char *equals = strchr(buffer, '=');
   if (equals == NULL)
      return false;
   *equals = '\0';
   char *key = buffer, *value = equals + 1;
   trim(key);
   trim(value);
   int number;
   bool flag;
   if (strcmp(key, "resolution") == 0) {
      if (!parse_name(value, resolution_names, AKENO_RES_COUNT, &number))
         return false;
      s->resolution = (akeno_resolution)number;
   } else if (strcmp(key, "frame_rate") == 0) {
      if (!parse_int(value, &number))
         return false;
      /* Only rates a display mode offers: 30 and 60 always, 120 where the
       * TV and param.json's high-frame-rate bit allow it. */
      if (number != 30 && number != 60 && number != 120) {
         clamped = true;
         number = number < 45 ? 30 : number < 90 ? 60 : 120;
      }
      s->frame_rate = number;
   } else if (strcmp(key, "vsync") == 0) {
      if (!parse_bool(value, &flag))
         return false;
      s->vsync = flag;
   } else if (strcmp(key, "graphics_quality") == 0) {
      if (!parse_int(value, &number))
         return false;
      s->graphics_quality = clamp(number, 0, 10, &clamped);
   } else if (strcmp(key, "audio_volume") == 0) {
      if (!parse_int(value, &number))
         return false;
      s->audio_volume = clamp(number, 0, 100, &clamped);
   } else if (strcmp(key, "stick_deadzone") == 0) {
      if (!parse_int(value, &number))
         return false;
      s->stick_deadzone = clamp(number, 0, 40, &clamped);
   } else if (strcmp(key, "camera_sensitivity") == 0) {
      if (!parse_int(value, &number))
         return false;
      s->camera_sensitivity = clamp(number, 10, 300, &clamped);
   } else if (strcmp(key, "invert_camera_y") == 0) {
      if (!parse_bool(value, &flag))
         return false;
      s->invert_camera_y = flag;
   } else if (strcmp(key, "touchpad") == 0) {
      if (!parse_name(value, touchpad_names, AKENO_TOUCHPAD_COUNT, &number))
         return false;
      s->touchpad = (akeno_touchpad_mode)number;
   } else if (strcmp(key, "verbose_log") == 0) {
      if (!parse_bool(value, &flag))
         return false;
      s->verbose_log = flag;
   } else {
      return false;
   }
   if (clamped_out != NULL)
      *clamped_out = clamped;
   return true;
}

int
akeno_settings_parse(akeno_settings *s, const char *text)
{
   int ignored = 0;
   while (text != NULL && *text) {
      const char *end = strchr(text, '\n');
      const size_t length = end ? (size_t)(end - text) : strlen(text);
      char line[256];
      if (length < sizeof(line)) {
         memcpy(line, text, length);
         line[length] = '\0';
         trim(line);
         if (line[0] != '\0' && line[0] != '#' && !akeno_settings_apply_line(s, line, NULL))
            ignored++;
      } else {
         ignored++;
      }
      text = end ? end + 1 : text + length;
   }
   return ignored;
}

int
akeno_settings_format(const akeno_settings *s, char *out, size_t size)
{
   const int n = snprintf(out, size,
                          "# AkenoBlox PS5 settings\n"
                          "resolution=%s\n"
                          "frame_rate=%d\n"
                          "vsync=%s\n"
                          "graphics_quality=%d\n"
                          "audio_volume=%d\n"
                          "stick_deadzone=%d\n"
                          "camera_sensitivity=%d\n"
                          "invert_camera_y=%s\n"
                          "touchpad=%s\n"
                          "verbose_log=%s\n",
                          akeno_resolution_name(s->resolution), s->frame_rate,
                          s->vsync ? "true" : "false", s->graphics_quality, s->audio_volume,
                          s->stick_deadzone, s->camera_sensitivity,
                          s->invert_camera_y ? "true" : "false", akeno_touchpad_name(s->touchpad),
                          s->verbose_log ? "true" : "false");
   return n < 0 || (size_t)n >= size ? -1 : n;
}

bool
akeno_settings_load(akeno_settings *s, const char *storage_root)
{
   char path[512];
   snprintf(path, sizeof(path), "%s/settings.ini", storage_root);
   FILE *f = fopen(path, "r");
   if (f == NULL)
      return false;
   char text[4096];
   const size_t length = fread(text, 1, sizeof(text) - 1, f);
   fclose(f);
   text[length] = '\0';
   akeno_settings_parse(s, text);
   return true;
}

bool
akeno_settings_save(const akeno_settings *s, const char *storage_root)
{
   char path[512], temporary[530], text[1024];
   if (akeno_settings_format(s, text, sizeof(text)) < 0)
      return false;
   snprintf(path, sizeof(path), "%s/settings.ini", storage_root);
   snprintf(temporary, sizeof(temporary), "%s.tmp", path);
   FILE *f = fopen(temporary, "w");
   if (f == NULL)
      return false;
   const size_t length = strlen(text);
   const bool written = fwrite(text, 1, length, f) == length;
   const bool closed = fclose(f) == 0;
   if (!written || !closed || rename(temporary, path) != 0) {
      remove(temporary);
      return false;
   }
   chmod(path, 0666);
   return true;
}
