/*
 * AkenoBlox PS5 - settings and log tests.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define _DEFAULT_SOURCE
#include "akeno_test.h"

#include "app/akeno_log.h"
#include "app/akeno_settings.h"

#include <stdlib.h>
#include <unistd.h>

static void
test_defaults_round_trip(void)
{
   akeno_settings a, b;
   akeno_settings_defaults(&a);
   CHECK_EQ_INT(a.frame_rate, 60);
   CHECK(a.vsync);
   CHECK_EQ_INT(a.resolution, AKENO_RES_1080P);
   char text[1024];
   REQUIRE(akeno_settings_format(&a, text, sizeof(text)) > 0);
   memset(&b, 0x5a, sizeof(b));
   akeno_settings_defaults(&b);
   b.audio_volume = 3;
   CHECK_EQ_INT(akeno_settings_parse(&b, text), 0);
   CHECK(akeno_settings_equal(&a, &b));
   char tiny[16];
   CHECK_EQ_INT(akeno_settings_format(&a, tiny, sizeof(tiny)), -1);
}

static void
test_parse_and_clamp(void)
{
   akeno_settings s;
   akeno_settings_defaults(&s);
   bool clamped;
   CHECK(akeno_settings_apply_line(&s, "  resolution = 3840x2160 ", &clamped) && !clamped);
   CHECK_EQ_INT(s.resolution, AKENO_RES_2160P);
   CHECK(akeno_settings_apply_line(&s, "audio_volume=250", &clamped) && clamped);
   CHECK_EQ_INT(s.audio_volume, 100);
   CHECK(akeno_settings_apply_line(&s, "frame_rate=75", &clamped) && clamped);
   CHECK_EQ_INT(s.frame_rate, 60);
   CHECK(akeno_settings_apply_line(&s, "frame_rate=144", &clamped) && clamped);
   CHECK_EQ_INT(s.frame_rate, 120);
   CHECK(akeno_settings_apply_line(&s, "vsync=off", &clamped));
   CHECK(!s.vsync);
   CHECK(akeno_settings_apply_line(&s, "touchpad=button", &clamped));
   CHECK_EQ_INT(s.touchpad, AKENO_TOUCHPAD_BUTTON);
   CHECK(!akeno_settings_apply_line(&s, "resolution=800x600", NULL));
   CHECK(!akeno_settings_apply_line(&s, "vsync=maybe", NULL));
   CHECK(!akeno_settings_apply_line(&s, "audio_volume=loud", NULL));
   CHECK(!akeno_settings_apply_line(&s, "audio_volume=99999999999999", NULL));
   CHECK(!akeno_settings_apply_line(&s, "unknown_key=1", NULL));
   CHECK(!akeno_settings_apply_line(&s, "no equals sign", NULL));
   CHECK_EQ_INT(akeno_settings_parse(&s, "# comment\n\nstick_deadzone=-5\nbogus\ncamera_sensitivity=1\n"), 1);
   CHECK_EQ_INT(s.stick_deadzone, 0);
   CHECK_EQ_INT(s.camera_sensitivity, 10);
}

static void
test_save_load(void)
{
   char dir[] = "/tmp/akeno-settings-XXXXXX";
   REQUIRE(mkdtemp(dir) != NULL);
   akeno_settings a, b;
   akeno_settings_defaults(&a);
   akeno_settings_defaults(&b);
   CHECK(!akeno_settings_load(&b, dir)); /* nothing saved yet */
   a.invert_camera_y = true;
   a.graphics_quality = 7;
   REQUIRE(akeno_settings_save(&a, dir));
   CHECK(akeno_settings_load(&b, dir));
   CHECK(akeno_settings_equal(&a, &b));
   char path[256];
   snprintf(path, sizeof(path), "%s/settings.ini.tmp", dir);
   CHECK(access(path, F_OK) != 0); /* the temporary was renamed away */
   snprintf(path, sizeof(path), "%s/settings.ini", dir);
   remove(path);
   rmdir(dir);
   CHECK(!akeno_settings_save(&a, "/nonexistent/akeno"));
}

static void
test_log_redaction(void)
{
   struct {
      const char *in, *out;
      int masked;
   } cases[] = {
      {"cookie: .ROBLOSECURITY=_|WARNING:-DO-NOT-SHARE|_abc123; path=/",
       "cookie: [redacted]", 1},
      {"login password=hunter2 user=bob", "login password=[redacted] user=bob", 1},
      {"Authorization: Bearer abc.def", "Authorization: [redacted]", 1},
      {"refresh_token=\"xyz\"", "refresh_token=\"[redacted]\"", 1},
      {"a token is just a word here", "a token is just a word here", 0},
      {"no secrets at all", "no secrets at all", 0},
   };
   for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
      char line[256];
      snprintf(line, sizeof(line), "%s", cases[i].in);
      const int masked = akeno_log_redact(line, sizeof(line));
      CHECK((masked >= 1) == (cases[i].masked >= 1));
      CHECK(strstr(line, "hunter2") == NULL);
      CHECK(strstr(line, "abc123") == NULL);
      CHECK(strstr(line, "abc.def") == NULL);
      CHECK_EQ_STR(line, cases[i].out);
   }
   /* The ROBLOSECURITY value never survives, whatever surrounds it. */
   char line[256] = "set .roblosecurity:SECRETVALUE and more";
   akeno_log_redact(line, sizeof(line));
   CHECK(strstr(line, "SECRETVALUE") == NULL);
}

static void
test_log_ring(void)
{
   for (int i = 0; i < AKENO_LOG_RING_LINES + 5; i++)
      akeno_log(AKENO_LOG_INFO, "line %d", i);
   akeno_log(AKENO_LOG_WARN, "password=shouldnotappear");
   char lines[AKENO_LOG_RING_LINES][AKENO_LOG_LINE_BYTES];
   const size_t n = akeno_log_recent(lines, AKENO_LOG_RING_LINES);
   CHECK_EQ_INT(n, AKENO_LOG_RING_LINES);
   CHECK(strstr(lines[n - 1], "[redacted]") != NULL);
   CHECK(strstr(lines[n - 1], "shouldnotappear") == NULL);
   CHECK(akeno_log_warning_count() >= 1);
}

static const akeno_test_case cases[] = {
   {"defaults_round_trip", test_defaults_round_trip},
   {"parse_and_clamp", test_parse_and_clamp},
   {"save_load", test_save_load},
   {"log_redaction", test_log_redaction},
   {"log_ring", test_log_ring},
};

AKENO_TEST_MAIN(cases)
