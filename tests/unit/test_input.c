/*
 * AkenoBlox PS5 - input tests: menu edges and repeat, and the DualSense as an
 * Android gamepad.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_test.h"

#include "input/akeno_input.h"

#include <math.h>

static akeno_pad_state
pad(uint32_t buttons)
{
   akeno_pad_state p = {.connected = true, .buttons = buttons};
   return p;
}

static void
test_edges(void)
{
   akeno_input_tracker t;
   akeno_input_tracker_reset(&t);
   akeno_pad_state p = pad(AKENO_BTN_CROSS);
   akeno_menu_input in = akeno_input_track(&t, &p, 0);
   CHECK(in.pressed & AKENO_BTN_CROSS);
   in = akeno_input_track(&t, &p, 16000);
   CHECK(!(in.pressed & AKENO_BTN_CROSS)); /* held: no second edge */
   CHECK(in.held & AKENO_BTN_CROSS);
   p = pad(0);
   in = akeno_input_track(&t, &p, 32000);
   CHECK(in.released & AKENO_BTN_CROSS);
   /* A disconnected pad holds nothing. */
   p = pad(AKENO_BTN_CIRCLE);
   p.connected = false;
   in = akeno_input_track(&t, &p, 48000);
   CHECK_EQ_INT(in.held, 0);
}

static void
test_dpad_repeat(void)
{
   akeno_input_tracker t;
   akeno_input_tracker_reset(&t);
   akeno_pad_state p = pad(AKENO_BTN_DOWN);
   int presses = 0;
   for (uint64_t now = 0; now <= 1000000; now += 10000) {
      const akeno_menu_input in = akeno_input_track(&t, &p, now);
      presses += (in.pressed & AKENO_BTN_DOWN) != 0;
   }
   /* One at 0, then repeats from 350 ms every 90 ms: 350, 440, ..., 980. */
   CHECK_EQ_INT(presses, 1 + 8);
   /* Cross never repeats. */
   akeno_input_tracker_reset(&t);
   p = pad(AKENO_BTN_CROSS);
   presses = 0;
   for (uint64_t now = 0; now <= 1000000; now += 10000)
      presses += (akeno_input_track(&t, &p, now).pressed & AKENO_BTN_CROSS) != 0;
   CHECK_EQ_INT(presses, 1);
}

static void
test_stick_navigation_hysteresis(void)
{
   akeno_input_tracker t;
   akeno_input_tracker_reset(&t);
   akeno_pad_state p = pad(0);
   p.left_y = 0.7f; /* down */
   akeno_menu_input in = akeno_input_track(&t, &p, 0);
   CHECK(in.pressed & AKENO_BTN_DOWN);
   p.left_y = 0.5f; /* between the thresholds: still engaged */
   in = akeno_input_track(&t, &p, 10000);
   CHECK(in.held & AKENO_BTN_DOWN);
   p.left_y = 0.3f; /* released */
   in = akeno_input_track(&t, &p, 20000);
   CHECK(in.released & AKENO_BTN_DOWN);
   p.left_y = 0.5f; /* not enough to engage again */
   in = akeno_input_track(&t, &p, 30000);
   CHECK(!(in.held & AKENO_BTN_DOWN));
   p.left_x = -0.9f;
   in = akeno_input_track(&t, &p, 40000);
   CHECK(in.pressed & AKENO_BTN_LEFT);
}

static void
test_default_mapping(void)
{
   akeno_gamepad_mapping m;
   akeno_gamepad_mapping_default(&m);
   CHECK_EQ_INT(m.keycode[4], AKENO_KEYCODE_BUTTON_A);      /* cross */
   CHECK_EQ_INT(m.keycode[5], AKENO_KEYCODE_BUTTON_B);      /* circle */
   CHECK_EQ_INT(m.keycode[6], AKENO_KEYCODE_BUTTON_X);      /* square */
   CHECK_EQ_INT(m.keycode[7], AKENO_KEYCODE_BUTTON_Y);      /* triangle */
   CHECK_EQ_INT(m.keycode[14], AKENO_KEYCODE_BUTTON_START); /* options */
   CHECK_EQ_INT(m.keycode[0], AKENO_KEYCODE_DPAD_UP);
   /* The Android SDK's values. */
   CHECK_EQ_INT(AKENO_KEYCODE_BUTTON_A, 96);
   CHECK_EQ_INT(AKENO_KEYCODE_BUTTON_START, 108);
   CHECK_EQ_INT(AKENO_AXIS_RZ, 14);
   CHECK_EQ_INT(AKENO_AXIS_LTRIGGER, 17);
}

static void
test_mapping_parse(void)
{
   akeno_gamepad_mapping m;
   akeno_gamepad_mapping_default(&m);
   CHECK(akeno_gamepad_mapping_parse(&m, "cross=BUTTON_B, circle=BUTTON_A,touchpad=NONE"));
   CHECK_EQ_INT(m.keycode[4], AKENO_KEYCODE_BUTTON_B);
   CHECK_EQ_INT(m.keycode[5], AKENO_KEYCODE_BUTTON_A);
   CHECK_EQ_INT(m.keycode[15], AKENO_KEYCODE_UNKNOWN);
   const akeno_gamepad_mapping before = m;
   CHECK(!akeno_gamepad_mapping_parse(&m, "cross=BUTTON_Q"));
   CHECK(!akeno_gamepad_mapping_parse(&m, "nosuch=BUTTON_A"));
   CHECK(!akeno_gamepad_mapping_parse(&m, "cross"));
   CHECK(!akeno_gamepad_mapping_parse(&m, "square=BUTTON_A,cross=BUTTON_Z"));
   CHECK(memcmp(&before, &m, sizeof(m)) == 0); /* a refused parse changes nothing */
   CHECK(akeno_gamepad_mapping_parse(&m, ""));
}

static float
axis(const akeno_android_gamepad *g, int which)
{
   for (size_t i = 0; i < g->axis_count; i++)
      if (g->axes[i].axis == which)
         return g->axes[i].value;
   return NAN;
}

static void
test_android_gamepad(void)
{
   akeno_gamepad_mapping m;
   akeno_gamepad_mapping_default(&m);
   akeno_pad_state p = pad(AKENO_BTN_CROSS | AKENO_BTN_R2 | AKENO_BTN_UP);
   p.left_x = 1.0f;
   p.left_y = 0.0f;
   p.right_x = 0.05f; /* inside a 12% dead zone */
   p.right_y = -0.05f;
   p.r2 = 0.75f;
   akeno_android_gamepad g;
   akeno_android_gamepad_from_pad(&p, &m, 12, &g);
   CHECK_EQ_INT(g.key_count, 3);
   CHECK_EQ_INT(g.keys_down[0], AKENO_KEYCODE_DPAD_UP); /* sorted */
   CHECK_EQ_INT(g.keys_down[1], AKENO_KEYCODE_BUTTON_A);
   CHECK_EQ_INT(g.keys_down[2], AKENO_KEYCODE_BUTTON_R2);
   CHECK(fabsf(axis(&g, AKENO_AXIS_X) - 1.0f) < 1e-5f); /* full deflection survives the dead zone */
   CHECK(axis(&g, AKENO_AXIS_Z) == 0.0f && axis(&g, AKENO_AXIS_RZ) == 0.0f);
   CHECK(fabsf(axis(&g, AKENO_AXIS_RTRIGGER) - 0.75f) < 1e-5f);
   CHECK(axis(&g, AKENO_AXIS_HAT_Y) == -1.0f);
   /* Half deflection past a 20% zone rescales: (0.5-0.2)/(0.8) = 0.375. */
   p = pad(0);
   p.left_y = 0.5f;
   akeno_android_gamepad_from_pad(&p, &m, 20, &g);
   CHECK(fabsf(axis(&g, AKENO_AXIS_Y) - 0.375f) < 1e-5f);
   /* Two buttons mapped to one keycode report it once. */
   CHECK(akeno_gamepad_mapping_parse(&m, "square=BUTTON_A"));
   p = pad(AKENO_BTN_CROSS | AKENO_BTN_SQUARE);
   akeno_android_gamepad_from_pad(&p, &m, 0, &g);
   CHECK_EQ_INT(g.key_count, 1);
   /* No pad, no keys. */
   p.connected = false;
   akeno_android_gamepad_from_pad(&p, &m, 0, &g);
   CHECK_EQ_INT(g.key_count, 0);
}

static void
test_key_edges(void)
{
   akeno_gamepad_mapping m;
   akeno_gamepad_mapping_default(&m);
   akeno_android_gamepad a, b;
   akeno_pad_state p = pad(AKENO_BTN_CROSS);
   akeno_android_gamepad_from_pad(&p, &m, 0, &a);
   p = pad(AKENO_BTN_CIRCLE);
   akeno_android_gamepad_from_pad(&p, &m, 0, &b);
   akeno_key_edge edges[8];
   const size_t n = akeno_android_key_edges(&a, &b, edges, 8);
   REQUIRE(n == 2);
   CHECK(edges[0].keycode == AKENO_KEYCODE_BUTTON_A && !edges[0].down); /* release first */
   CHECK(edges[1].keycode == AKENO_KEYCODE_BUTTON_B && edges[1].down);
   CHECK_EQ_INT(akeno_android_key_edges(&a, &a, edges, 8), 0);
}

static const akeno_test_case cases[] = {
   {"edges", test_edges},
   {"dpad_repeat", test_dpad_repeat},
   {"stick_navigation_hysteresis", test_stick_navigation_hysteresis},
   {"default_mapping", test_default_mapping},
   {"mapping_parse", test_mapping_parse},
   {"android_gamepad", test_android_gamepad},
   {"key_edges", test_key_edges},
};

AKENO_TEST_MAIN(cases)
