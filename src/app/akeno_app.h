/*
 * AkenoBlox PS5 - the launcher program: start-up, the frame loop, shut-down.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef AKENO_APP_H
#define AKENO_APP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct akeno_app_options {
   uint64_t max_frames;        /* stop after this many frames; 0 runs until Exit */
   bool stop_when_script_done; /* host: stop a few frames after the input script ends */
   bool fast;                  /* host: do not pace frames */
   bool run_probes_at_start;   /* run the Diagnostics probes before the first frame */
   const char *screenshot_path; /* host: the last frame, as PPM */
   const char *report_path;     /* write the diagnostics report here at the end */
} akeno_app_options;

/* Returns the process status: 0, or non-zero when start-up failed or a
 * requested probe failed. */
int akeno_app_run(const akeno_app_options *options);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_APP_H */
