/*
 * AkenoBlox PS5 - the DualSense, player 0.
 * Copyright (C) 2026 AkenoBlox contributors
 * Portions adapted from PS5_VulkanTemplate ps5/src/platform.c,
 * Copyright (C) 2026 Mihawk, MIT licence (see NOTICE.md).
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The sample layout, the button numbering and the open-with-retries are the
 * ones PS5_vkQuake, PS5_RetroArch and ProsperoLight read on hardware, as the
 * template records them. The console's numbering is translated here, once,
 * into AKENO_BTN_*.
 */
#include "ps5_internal.h"

#include <stddef.h>
#include <string.h>

struct pad_sample {
   uint32_t buttons;
   uint8_t left_x, left_y, right_x, right_y, l2, r2;
   uint8_t reserved[66];
   int32_t connected;
   uint64_t timestamp_us;
   uint8_t extension[16];
   uint8_t connected_count;
   uint8_t remaining[15];
};
_Static_assert(sizeof(struct pad_sample) == 120, "the console's pad samples are 120 bytes");
_Static_assert(offsetof(struct pad_sample, connected) == 0x4c, "connection state sits at 0x4c");
_Static_assert(offsetof(struct pad_sample, timestamp_us) == 0x50, "the timestamp sits at 0x50");

/* The console's button bits (PS5_VulkanTemplate platform-contracts.md). */
enum {
   PAD_L3 = 0x000002,
   PAD_R3 = 0x000004,
   PAD_OPTIONS = 0x000008,
   PAD_UP = 0x000010,
   PAD_RIGHT = 0x000020,
   PAD_DOWN = 0x000040,
   PAD_LEFT = 0x000080,
   PAD_L1 = 0x000400,
   PAD_R1 = 0x000800,
   PAD_TRIANGLE = 0x001000,
   PAD_CIRCLE = 0x002000,
   PAD_CROSS = 0x004000,
   PAD_SQUARE = 0x008000,
   PAD_TOUCH_PAD = 0x100000,
   PAD_INTERCEPTED = (int)0x80000000u, /* the shell has the pad */
};

static const struct {
   uint32_t console;
   uint32_t akeno;
} button_map[] = {
   {PAD_UP, AKENO_BTN_UP},         {PAD_DOWN, AKENO_BTN_DOWN},     {PAD_LEFT, AKENO_BTN_LEFT},
   {PAD_RIGHT, AKENO_BTN_RIGHT},   {PAD_CROSS, AKENO_BTN_CROSS},   {PAD_CIRCLE, AKENO_BTN_CIRCLE},
   {PAD_SQUARE, AKENO_BTN_SQUARE}, {PAD_TRIANGLE, AKENO_BTN_TRIANGLE},
   {PAD_L1, AKENO_BTN_L1},         {PAD_R1, AKENO_BTN_R1},         {PAD_L3, AKENO_BTN_L3},
   {PAD_R3, AKENO_BTN_R3},         {PAD_OPTIONS, AKENO_BTN_OPTIONS},
   {PAD_TOUCH_PAD, AKENO_BTN_TOUCHPAD},
};

static int32_t pad_handle = -1;
static int32_t pad_user = -1;

bool
akeno_ps5_pad_open(void)
{
   if (pad_handle >= 0)
      return true;
   if (scePadInit() < 0)
      return false;
   int32_t user = -1;
   if (sceUserServiceGetInitialUser(&user) < 0 || user < 0)
      return false;
   /* A title can start before the pad service has published the device. */
   int32_t handle = -1;
   for (int attempt = 0; attempt < 10 && handle < 0; attempt++) {
      handle = scePadOpen(user, 0, 0, NULL);
      if (handle < 0)
         sceKernelUsleep(100000);
   }
   if (handle < 0)
      handle = scePadGetHandle(user, 0, 0);
   if (handle < 0)
      return false;
   pad_handle = handle;
   pad_user = user;
   return true;
}

void
akeno_ps5_pad_close(void)
{
   if (pad_handle >= 0)
      scePadClose(pad_handle);
   pad_handle = -1;
   pad_user = -1;
}

static float
stick(uint8_t value)
{
   const float v = ((float)value - 128.0f) / 127.0f;
   return v < -1.0f ? -1.0f : v > 1.0f ? 1.0f : v;
}

bool
akeno_platform_pad_poll(akeno_pad_state *state)
{
   memset(state, 0, sizeof(*state));
   if (pad_handle < 0) {
      /* Try again now and then: a controller paired after start-up. */
      static uint32_t polls;
      if (++polls % 120 != 0 || !akeno_ps5_pad_open())
         return false;
   }
   struct pad_sample samples[16];
   const int count = scePadRead(pad_handle, samples, 16);
   if (count <= 0)
      return state->connected;
   const struct pad_sample *s = &samples[count - 1];
   state->connected = s->connected != 0;
   state->timestamp_us = s->timestamp_us;
   if ((s->buttons & (uint32_t)PAD_INTERCEPTED) != 0)
      return state->connected; /* the home screen or a dialog has the pad */
   for (size_t i = 0; i < sizeof(button_map) / sizeof(button_map[0]); i++)
      if (s->buttons & button_map[i].console)
         state->buttons |= button_map[i].akeno;
   state->left_x = stick(s->left_x);
   state->left_y = stick(s->left_y);
   state->right_x = stick(s->right_x);
   state->right_y = stick(s->right_y);
   state->l2 = (float)s->l2 / 255.0f;
   state->r2 = (float)s->r2 / 255.0f;
   /* L2 and R2 as buttons from the analog triggers, rather than from bits the
    * template does not document. */
   if (s->l2 > 127)
      state->buttons |= AKENO_BTN_L2;
   if (s->r2 > 127)
      state->buttons |= AKENO_BTN_R2;
   return state->connected;
}
