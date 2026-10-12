/*
 * AkenoBlox PS5 - the entry point.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * On the console main returns to PS5_Vulkan's CRT, which calls
 * catchReturnFromMain (src/platform/ps5/platform_ps5.c): the title asks the
 * shell to close it rather than exiting, which the console would report as a
 * crash. The console hands argv without a program name in front
 * (PS5_VulkanTemplate platform-contracts.md), and a launch from the home
 * screen passes argv[0] "", so nothing here reads argv there.
 */
#include "app/akeno_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(AKENO_PS5)
#include "akeno_host.h"
#include "akeno_version.h"

static void
usage(void)
{
   fputs("usage: akenoblox-host [options]\n"
         "  --script STEPS      scripted input, e.g. \"down,cross,wait:5,circle\"\n"
         "  --frames N          stop after N frames\n"
         "  --fast              do not pace frames to 60 Hz\n"
         "  --screenshot FILE   write the last frame as PPM\n"
         "  --storage DIR       settings, logs and reports (default ./akeno-data)\n"
         "  --diagnostics FILE  run the hardware probes, write the report, and exit\n"
         "  --version           print the version\n",
         stderr);
}

int
main(int argc, char **argv)
{
   akeno_app_options opt = {0};
   for (int i = 1; i < argc; i++) {
      const char *arg = argv[i];
      const char *next = i + 1 < argc ? argv[i + 1] : NULL;
      if (strcmp(arg, "--script") == 0 && next) {
         if (!akeno_host_set_input_script(next)) {
            fprintf(stderr, "akenoblox-host: bad --script\n");
            return 2;
         }
         opt.stop_when_script_done = true;
         i++;
      } else if (strcmp(arg, "--frames") == 0 && next) {
         opt.max_frames = strtoull(next, NULL, 10);
         i++;
      } else if (strcmp(arg, "--fast") == 0) {
         opt.fast = true;
      } else if (strcmp(arg, "--screenshot") == 0 && next) {
         opt.screenshot_path = next;
         i++;
      } else if (strcmp(arg, "--storage") == 0 && next) {
         akeno_host_set_storage_root(next);
         i++;
      } else if (strcmp(arg, "--diagnostics") == 0 && next) {
         opt.run_probes_at_start = true;
         opt.report_path = next;
         opt.max_frames = 1;
         opt.fast = true;
         i++;
      } else if (strcmp(arg, "--version") == 0) {
         puts(AKENO_VERSION_STRING);
         return 0;
      } else {
         usage();
         return strcmp(arg, "--help") == 0 ? 0 : 2;
      }
   }
   return akeno_app_run(&opt);
}

#else

int
main(int argc, char **argv)
{
   (void)argc;
   (void)argv;
   const akeno_app_options opt = {0};
   return akeno_app_run(&opt);
}

#endif
