/*
 * AkenoBlox PS5 - input.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_input.h"

#include <string.h>

/* Indexed by bit position of AKENO_BTN_*. */
static const char *const button_names[AKENO_BTN_COUNT] = {
   "up", "down", "left", "right", "cross", "circle", "square", "triangle",
   "l1", "r1",   "l2",   "r2",    "l3",    "r3",     "options", "touchpad",
};

static const struct {
   const char *name;
   int keycode;
} keycode_names[] = {
   {"NONE", AKENO_KEYCODE_UNKNOWN},
   {"DPAD_UP", AKENO_KEYCODE_DPAD_UP},
   {"DPAD_DOWN", AKENO_KEYCODE_DPAD_DOWN},
   {"DPAD_LEFT", AKENO_KEYCODE_DPAD_LEFT},
   {"DPAD_RIGHT", AKENO_KEYCODE_DPAD_RIGHT},
   {"BUTTON_A", AKENO_KEYCODE_BUTTON_A},
   {"BUTTON_B", AKENO_KEYCODE_BUTTON_B},
   {"BUTTON_X", AKENO_KEYCODE_BUTTON_X},
   {"BUTTON_Y", AKENO_KEYCODE_BUTTON_Y},
   {"BUTTON_L1", AKENO_KEYCODE_BUTTON_L1},
   {"BUTTON_R1", AKENO_KEYCODE_BUTTON_R1},
   {"BUTTON_L2", AKENO_KEYCODE_BUTTON_L2},
   {"BUTTON_R2", AKENO_KEYCODE_BUTTON_R2},
   {"BUTTON_THUMBL", AKENO_KEYCODE_BUTTON_THUMBL},
   {"BUTTON_THUMBR", AKENO_KEYCODE_BUTTON_THUMBR},
   {"BUTTON_START", AKENO_KEYCODE_BUTTON_START},
   {"BUTTON_SELECT", AKENO_KEYCODE_BUTTON_SELECT},
};

const char *
akeno_button_name(unsigned index)
{
   return index < AKENO_BTN_COUNT ? button_names[index] : "unknown";
}

const char *
akeno_keycode_name(int keycode)
{
   for (size_t i = 0; i < sizeof(keycode_names) / sizeof(keycode_names[0]); i++)
      if (keycode_names[i].keycode == keycode)
         return keycode_names[i].name;
   return "UNKNOWN";
}

/* ------------------------------------------------------------ the menu */

void
akeno_input_tracker_reset(akeno_input_tracker *t)
{
   memset(t, 0, sizeof(*t));
}

/* Engage past 0.6, let go under 0.4: a stick resting near the threshold does
 * not chatter. */
static bool
stick_direction(bool *engaged, float value)
{
   *engaged = *engaged ? value > 0.4f : value > 0.6f;
   return *engaged;
}

akeno_menu_input
akeno_input_track(akeno_input_tracker *t, const akeno_pad_state *pad, uint64_t now_us)
{
   uint32_t held = 0;
   if (pad != NULL && pad->connected) {
      held = pad->buttons;
      if (stick_direction(&t->stick_engaged[0], -pad->left_y))
         held |= AKENO_BTN_UP;
      if (stick_direction(&t->stick_engaged[1], pad->left_y))
         held |= AKENO_BTN_DOWN;
      if (stick_direction(&t->stick_engaged[2], -pad->left_x))
         held |= AKENO_BTN_LEFT;
      if (stick_direction(&t->stick_engaged[3], pad->left_x))
         held |= AKENO_BTN_RIGHT;
   } else {
      memset(t->stick_engaged, 0, sizeof(t->stick_engaged));
   }
   akeno_menu_input in = {
      .held = held,
      .pressed = held & ~t->previous,
      .released = t->previous & ~held,
   };
   const uint32_t repeating = AKENO_BTN_UP | AKENO_BTN_DOWN | AKENO_BTN_LEFT | AKENO_BTN_RIGHT;
   for (unsigned bit = 0; bit < AKENO_BTN_COUNT; bit++) {
      const uint32_t mask = 1u << bit;
      if (!(repeating & mask))
         continue;
      if (in.pressed & mask) {
         t->repeat_due_us[bit] = now_us + AKENO_REPEAT_DELAY_US;
      } else if ((held & mask) && now_us >= t->repeat_due_us[bit]) {
         in.pressed |= mask;
         t->repeat_due_us[bit] = now_us + AKENO_REPEAT_INTERVAL_US;
      }
   }
   t->previous = held;
   return in;
}

/* ------------------------------------------------------- the gamepad */

void
akeno_gamepad_mapping_default(akeno_gamepad_mapping *m)
{
   static const int defaults[AKENO_BTN_COUNT] = {
      AKENO_KEYCODE_DPAD_UP,      AKENO_KEYCODE_DPAD_DOWN,     AKENO_KEYCODE_DPAD_LEFT,
      AKENO_KEYCODE_DPAD_RIGHT,   AKENO_KEYCODE_BUTTON_A,      AKENO_KEYCODE_BUTTON_B,
      AKENO_KEYCODE_BUTTON_X,     AKENO_KEYCODE_BUTTON_Y,      AKENO_KEYCODE_BUTTON_L1,
      AKENO_KEYCODE_BUTTON_R1,    AKENO_KEYCODE_BUTTON_L2,     AKENO_KEYCODE_BUTTON_R2,
      AKENO_KEYCODE_BUTTON_THUMBL, AKENO_KEYCODE_BUTTON_THUMBR, AKENO_KEYCODE_BUTTON_START,
      AKENO_KEYCODE_BUTTON_SELECT,
   };
   memcpy(m->keycode, defaults, sizeof(defaults));
}

static int
find_button(const char *name, size_t length)
{
   for (int i = 0; i < AKENO_BTN_COUNT; i++)
      if (strlen(button_names[i]) == length && memcmp(button_names[i], name, length) == 0)
         return i;
   return -1;
}

static bool
find_keycode(const char *name, size_t length, int *keycode)
{
   for (size_t i = 0; i < sizeof(keycode_names) / sizeof(keycode_names[0]); i++)
      if (strlen(keycode_names[i].name) == length && memcmp(keycode_names[i].name, name, length) == 0) {
         *keycode = keycode_names[i].keycode;
         return true;
      }
   return false;
}

bool
akeno_gamepad_mapping_parse(akeno_gamepad_mapping *mapping, const char *text)
{
   if (mapping == NULL || text == NULL)
      return false;
   akeno_gamepad_mapping next = *mapping;
   const char *at = text;
   while (*at) {
      while (*at == ' ' || *at == ',')
         at++;
      if (*at == '\0')
         break;
      const char *equals = strchr(at, '=');
      const char *comma = strchr(at, ',');
      if (equals == NULL || (comma != NULL && comma < equals))
         return false;
      const char *value = equals + 1;
      const char *end = comma ? comma : value + strlen(value);
      size_t value_length = (size_t)(end - value);
      while (value_length > 0 && value[value_length - 1] == ' ')
         value_length--;
      const int button = find_button(at, (size_t)(equals - at));
      int keycode;
      if (button < 0 || !find_keycode(value, value_length, &keycode))
         return false;
      next.keycode[button] = keycode;
      at = end;
   }
   *mapping = next;
   return true;
}

static void
deadzone(float x, float y, float zone, float *out_x, float *out_y)
{
   const float magnitude = __builtin_sqrtf(x * x + y * y);
   if (magnitude <= zone || magnitude == 0.0f) {
      *out_x = *out_y = 0.0f;
      return;
   }
   float scaled = (magnitude - zone) / (1.0f - zone);
   if (scaled > 1.0f)
      scaled = 1.0f;
   *out_x = x / magnitude * scaled;
   *out_y = y / magnitude * scaled;
}

static float
clamp01(float v)
{
   return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

void
akeno_android_gamepad_from_pad(const akeno_pad_state *pad, const akeno_gamepad_mapping *mapping,
                               int deadzone_percent, akeno_android_gamepad *out)
{
   memset(out, 0, sizeof(*out));
   if (pad == NULL || !pad->connected || mapping == NULL)
      return;
   for (unsigned bit = 0; bit < AKENO_BTN_COUNT; bit++) {
      if (!(pad->buttons & (1u << bit)))
         continue;
      const int keycode = mapping->keycode[bit];
      if (keycode == AKENO_KEYCODE_UNKNOWN)
         continue;
      /* Insertion into the sorted set. */
      size_t i = 0;
      while (i < out->key_count && out->keys_down[i] < keycode)
         i++;
      if (i < out->key_count && out->keys_down[i] == keycode)
         continue;
      memmove(&out->keys_down[i + 1], &out->keys_down[i], (out->key_count - i) * sizeof(int));
      out->keys_down[i] = keycode;
      out->key_count++;
   }
   const float zone = (float)(deadzone_percent < 0 ? 0 : deadzone_percent > 40 ? 40 : deadzone_percent) /
                      100.0f;
   float lx, ly, rx, ry;
   deadzone(pad->left_x, pad->left_y, zone, &lx, &ly);
   deadzone(pad->right_x, pad->right_y, zone, &rx, &ry);
   const float hat_x = (pad->buttons & AKENO_BTN_RIGHT ? 1.0f : 0.0f) -
                       (pad->buttons & AKENO_BTN_LEFT ? 1.0f : 0.0f);
   const float hat_y = (pad->buttons & AKENO_BTN_DOWN ? 1.0f : 0.0f) -
                       (pad->buttons & AKENO_BTN_UP ? 1.0f : 0.0f);
   const akeno_android_axis axes[] = {
      {AKENO_AXIS_X, lx},
      {AKENO_AXIS_Y, ly},
      {AKENO_AXIS_Z, rx},
      {AKENO_AXIS_RZ, ry},
      {AKENO_AXIS_HAT_X, hat_x},
      {AKENO_AXIS_HAT_Y, hat_y},
      {AKENO_AXIS_LTRIGGER, clamp01(pad->l2)},
      {AKENO_AXIS_RTRIGGER, clamp01(pad->r2)},
   };
   memcpy(out->axes, axes, sizeof(axes));
   out->axis_count = sizeof(axes) / sizeof(axes[0]);
}

static bool
contains(const akeno_android_gamepad *g, int keycode)
{
   for (size_t i = 0; i < g->key_count; i++)
      if (g->keys_down[i] == keycode)
         return true;
   return false;
}

size_t
akeno_android_key_edges(const akeno_android_gamepad *previous, const akeno_android_gamepad *current,
                        akeno_key_edge *edges, size_t max)
{
   size_t n = 0;
   for (size_t i = 0; i < previous->key_count && n < max; i++)
      if (!contains(current, previous->keys_down[i]))
         edges[n++] = (akeno_key_edge){previous->keys_down[i], false};
   for (size_t i = 0; i < current->key_count && n < max; i++)
      if (!contains(previous, current->keys_down[i]))
         edges[n++] = (akeno_key_edge){current->keys_down[i], true};
   return n;
}
