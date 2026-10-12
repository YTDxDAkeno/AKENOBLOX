/*
 * AkenoBlox PS5 - the platform layer: everything the app and the runtime ask
 * of the machine they run on.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Two implementations: src/platform/host (a POSIX host, used for development
 * and every test CI runs) and src/platform/ps5 (the console, through the
 * payload SDK's exported functions and PS5_PayloadSDK's platform layer). The
 * app and the compatibility runtime include only this header, so console
 * constants and Sony function names never leak above it
 * (PS5_VulkanTemplate platform-contracts.md: "Translate the console's
 * numbering once, at the platform boundary").
 */
#ifndef AKENO_PLATFORM_H
#define AKENO_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "akeno_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------ lifecycle */

/* Called first, once. title names this program in the console's log. */
int akeno_platform_init(const char *title);
/* Releases the display, the pad and anything else init or later calls took. */
void akeno_platform_shutdown(void);
/* Ends the program the way the platform requires. On the console that is
 * asking the shell to close the title; exit() there is reported as a crash
 * (PS5_VulkanTemplate platform-contracts.md, "Ending a title"). */
_Noreturn void akeno_platform_exit(int status);

/* --------------------------------------------------------------- facts */

typedef struct akeno_platform_info {
   char platform[24];       /* "linux-host", "ps5" */
   char firmware[32];       /* the system software version, or "unknown" */
   bool memory_known;
   uint64_t memory_total;   /* host: MemTotal; console: direct memory */
   uint64_t memory_free;    /* host: MemAvailable; console: free direct memory */
   uint64_t memory_largest; /* console: largest free direct range; host: 0 */
   uint64_t flexible_free;  /* console: flexible memory free; host: 0 */
   size_t page_size;        /* what code memory protects in */
} akeno_platform_info;

void akeno_platform_query(akeno_platform_info *info);

/* -------------------------------------------------------------- memory */

/* Code memory for loaders (akeno_mem.h). */
const akeno_mem_ops *akeno_platform_code_memory(void);

/* ---------------------------------------------------------------- time */

uint64_t akeno_platform_now_us(void);
void akeno_platform_sleep_us(uint32_t microseconds);

/* ------------------------------------------------------------- logging */

/* One finished line to the platform's log (standard error on a host, klog on
 * the console). No newline needed. */
void akeno_platform_log_line(const char *line);

/* ------------------------------------------------------------ storage */

/* A writable directory for settings, logs and caches, created on first use,
 * and the read-only directory the program's own files are in. */
const char *akeno_platform_storage_root(void);
const char *akeno_platform_app_root(void);

/* ---------------------------------------------------------------- input */

/* Platform-neutral button bits. */
enum {
   AKENO_BTN_UP = 1u << 0,
   AKENO_BTN_DOWN = 1u << 1,
   AKENO_BTN_LEFT = 1u << 2,
   AKENO_BTN_RIGHT = 1u << 3,
   AKENO_BTN_CROSS = 1u << 4,
   AKENO_BTN_CIRCLE = 1u << 5,
   AKENO_BTN_SQUARE = 1u << 6,
   AKENO_BTN_TRIANGLE = 1u << 7,
   AKENO_BTN_L1 = 1u << 8,
   AKENO_BTN_R1 = 1u << 9,
   AKENO_BTN_L2 = 1u << 10,
   AKENO_BTN_R2 = 1u << 11,
   AKENO_BTN_L3 = 1u << 12,
   AKENO_BTN_R3 = 1u << 13,
   AKENO_BTN_OPTIONS = 1u << 14,
   AKENO_BTN_TOUCHPAD = 1u << 15,
   AKENO_BTN_COUNT = 16
};

typedef struct akeno_pad_state {
   bool connected;
   uint32_t buttons;            /* AKENO_BTN_* held now */
   float left_x, left_y;        /* -1..1, y down-positive, no dead zone applied */
   float right_x, right_y;
   float l2, r2;                /* 0..1 */
   uint64_t timestamp_us;
} akeno_pad_state;

/* Player 0's controller now. false when the platform has no pad at all (the
 * host without a script) or the shell has it (the home screen, a dialog). */
bool akeno_platform_pad_poll(akeno_pad_state *state);

/* -------------------------------------------------------------- display */

/* A linear frame of 0xAARRGGBB pixels (bytes B, G, R, A in memory). */
typedef struct akeno_frame {
   uint32_t *pixels;
   unsigned width, height;
   unsigned stride; /* pixels per row */
   uint64_t serial; /* changes whenever the pixels do: a presenter may skip
                     * copying a frame it already holds */
} akeno_frame;

/* Takes the display at width x height. On the console the launcher presents
 * CPU-drawn frames through VideoOut, the bring-up path PS5_Vulkan's demo
 * renderer proved; the engine later owns the display through RADV's
 * VK_KHR_display swapchain in a process of its own (docs/DECISIONS.md). */
int akeno_platform_display_open(unsigned width, unsigned height, char *err, size_t err_size);
/* Shows frame at the next vertical blank; blocks for it on the console. */
int akeno_platform_display_present(const akeno_frame *frame);
void akeno_platform_display_close(void);

/* -------------------------------------------------------------- network */

/* Whether host resolves, as a one-line summary. Only a DNS query; nothing is
 * sent to the host itself. Returns 0 when it resolved. */
int akeno_platform_network_check(const char *host, char *summary, size_t summary_size);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_PLATFORM_H */
