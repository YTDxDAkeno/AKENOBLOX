/*
 * AkenoBlox PS5 - the launcher program.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The program must start without Roblox and must never pretend to run it.
 * Everything that touches hardware beyond the display and the pad (CPUID, the
 * %fs probe, executing loaded code) runs only when the user asks on the
 * Diagnostics screen, logged before it runs, so a console that faults in one
 * of them leaves a log naming which.
 */
#include "akeno_app.h"

#include "akeno_version.h"
#include "app/akeno_log.h"
#include "app/akeno_menu.h"
#include "app/akeno_settings.h"
#include "diagnostics/akeno_cpu.h"
#include "diagnostics/akeno_report.h"
#include "diagnostics/akeno_selftest.h"
#include "diagnostics/akeno_udp.h"
#include "input/akeno_input.h"
#include "runtime/akeno_runtime.h"
#include "ui/akeno_screens.h"

#if !defined(AKENO_PS5)
#include "akeno_host.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct app {
   akeno_settings settings;
   akeno_menu menu;
   akeno_input_tracker tracker;
   akeno_gamepad_mapping mapping;
   akeno_platform_info platform;
   akeno_report report;
   akeno_cpu_info cpu;
   akeno_fs_probe fs_probe;
   akeno_selftest_result selftest;
   akeno_package_check package;
   bool probes_ran;
   bool display_open;
   bool pad_connected;
   int storage_check; /* -1 not run, 0 failed, 1 passed */
   char storage_detail[96];
   int network_check; /* -1 not run, 0 failed, 1 passed */
   char network_detail[128];
   int dns_control_check; /* -1 not run, 0 failed, 1 resolved */
   char dns_control_detail[128];
   int udp_check; /* -1 not run, 0 unavailable, 1 loopback passed */
   char udp_detail[128];
   char toast[96];
   uint64_t toast_until_us;
   akeno_frame frame;
   const char *storage;
} app;

static void
toast(app *a, uint64_t now, const char *message)
{
   snprintf(a->toast, sizeof(a->toast), "%s", message);
   a->toast_until_us = now + 3000000u;
   AKENO_INFO("notice: %s", message);
}

static void
rebuild_report(app *a)
{
   akeno_report_clear(&a->report);
   akeno_report_build(&a->report);
   akeno_platform_query(&a->platform);
   akeno_report_system(&a->report, &a->platform);
   akeno_report_add(&a->report, "Launcher", a->display_open ? AKENO_ROW_OK : AKENO_ROW_PENDING,
                    "Display",
#if defined(AKENO_PS5)
                    "%s", a->display_open ? "VideoOut, CPU-drawn 1920x1080" : "not open");
#else
                    "%s", a->display_open ? "host frame buffer 1920x1080" : "not open");
#endif
   akeno_report_add(&a->report, "Launcher", a->pad_connected ? AKENO_ROW_OK : AKENO_ROW_WARN,
                    "Controller", "%s", a->pad_connected ? "connected" : "none seen");
   akeno_report_add(&a->report, "Launcher",
                    a->storage_check < 0   ? AKENO_ROW_PENDING
                    : a->storage_check > 0 ? AKENO_ROW_OK
                                           : AKENO_ROW_FAIL,
                    "Storage", "%s", a->storage_check < 0 ? "not checked" : a->storage_detail);
   akeno_report_add(&a->report, "Launcher",
                    a->network_check < 0   ? AKENO_ROW_PENDING
                    : a->network_check > 0 ? AKENO_ROW_OK
                                           : AKENO_ROW_WARN,
                    "Network (DNS only)", "%s",
                    a->network_check < 0 ? "not checked" : a->network_detail);
   /* A control domain distinguishes "the resolver cannot resolve anything"
    * from a domain-specific failure. Neither lookup contacts a Sony host. */
   akeno_report_add(&a->report, "Network",
                    a->dns_control_check < 0   ? AKENO_ROW_PENDING
                    : a->dns_control_check > 0 ? AKENO_ROW_OK
                                               : AKENO_ROW_WARN,
                    "DNS control (example.com)", "%s",
                    a->dns_control_check < 0 ? "not checked" : a->dns_control_detail);
   akeno_report_add(&a->report, "Network",
                    a->udp_check < 0   ? AKENO_ROW_PENDING
                    : a->udp_check > 0 ? AKENO_ROW_OK
                                       : AKENO_ROW_WARN,
                    "UDP IPv4 loopback", "%s",
                    a->udp_check < 0 ? "not checked (external UDP unverified)" : a->udp_detail);
   akeno_report_cpu(&a->report, &a->cpu);
   akeno_report_fs_probe(&a->report, &a->fs_probe);
   akeno_report_selftest(&a->report, &a->selftest);
}

/* A file written, read back and removed under storage: the logs, settings and
 * the engine's caches all depend on it. */
static void
check_storage(app *a)
{
   char path[512];
   snprintf(path, sizeof(path), "%s/.akeno-write-test", a->storage);
   static const char probe[] = "akenoblox storage probe";
   char back[sizeof(probe)] = "";
   FILE *f = fopen(path, "w");
   bool ok = f != NULL && fwrite(probe, 1, sizeof(probe), f) == sizeof(probe);
   if (f != NULL)
      ok = fclose(f) == 0 && ok;
   f = ok ? fopen(path, "r") : NULL;
   ok = f != NULL && fread(back, 1, sizeof(back), f) == sizeof(back) && memcmp(back, probe, sizeof(probe)) == 0;
   if (f != NULL)
      fclose(f);
   remove(path);
   a->storage_check = ok ? 1 : 0;
   snprintf(a->storage_detail, sizeof(a->storage_detail), "%s: %s", ok ? "writable" : "NOT writable",
            a->storage);
}

static void
run_probes(app *a)
{
   AKENO_INFO("probes: CPUID");
   akeno_cpu_query(&a->cpu);
   AKENO_INFO("probes: CPUID done: %s, x86-64-v%d", a->cpu.brand[0] ? a->cpu.brand : a->cpu.vendor,
              a->cpu.level);
   AKENO_INFO("probes: %%fs:0x28");
   akeno_fs_probe_run(&a->fs_probe);
   AKENO_INFO("probes: %%fs:0x28 main %s, thread %s", a->fs_probe.main_stable ? "stable" : "changed",
              a->fs_probe.thread_ran ? (a->fs_probe.thread_stable ? "stable" : "changed")
                                     : "not run");
   AKENO_INFO("probes: storage");
   check_storage(a);
   AKENO_INFO("probes: network (DNS lookup only)");
   a->network_check = akeno_platform_network_check("www.roblox.com", a->network_detail,
                                                    sizeof(a->network_detail)) == 0;
   AKENO_INFO("probes: network: %s", a->network_detail);
   AKENO_INFO("probes: DNS control (example.com, lookup only)");
   a->dns_control_check = akeno_platform_network_check("example.com", a->dns_control_detail,
                                                       sizeof(a->dns_control_detail)) == 0;
   AKENO_INFO("probes: DNS control: %s", a->dns_control_detail);
   AKENO_INFO("probes: UDP IPv4 loopback (local socket only)");
   a->udp_check = akeno_udp_loopback_probe(a->udp_detail, sizeof(a->udp_detail)) == 0;
   AKENO_INFO("probes: UDP loopback: %s", a->udp_detail);
   AKENO_INFO("probes: ELF loader self-test");
   akeno_selftest_run(akeno_platform_code_memory(), true, &a->selftest);
   a->probes_ran = true;
   rebuild_report(a);
}

static void
apply_settings(app *a)
{
   akeno_log_set_level(a->settings.verbose_log ? AKENO_LOG_DEBUG : AKENO_LOG_INFO);
}

static akeno_command
step_menu(app *a, const akeno_pad_state *pad, uint64_t now, bool *changed)
{
   const akeno_menu_input in = akeno_input_track(&a->tracker, pad, now);
   const akeno_screen before = a->menu.screen;
   const akeno_command command = akeno_menu_update(&a->menu, &in, now);
   *changed = in.pressed || in.released || command != AKENO_CMD_NONE || before != a->menu.screen ||
              a->menu.screen == AKENO_SCREEN_INPUT_TEST;
   return command;
}

static bool
execute(app *a, akeno_command command, uint64_t now)
{
   char message[160];
   switch (command) {
   case AKENO_CMD_NONE: break;
   case AKENO_CMD_RUN_PROBES:
      toast(a, now, "Running hardware probes...");
      run_probes(a);
      snprintf(message, sizeof(message), "Probes done: loader self-test %s",
               a->selftest.passed ? "PASSED" : "FAILED");
      toast(a, akeno_platform_now_us(), message);
      break;
   case AKENO_CMD_EXPORT_REPORT: {
      char path[512];
      snprintf(path, sizeof(path), "%s/diagnostics.txt", a->storage);
      const bool saved = akeno_report_write(&a->report, path) == 0;
      snprintf(message, sizeof(message), saved ? "Saved %s" : "Could not save %s", path);
      toast(a, now, message);
      break;
   }
   case AKENO_CMD_CHECK_PACKAGE:
      akeno_package_check_run(a->storage, &a->package);
      AKENO_INFO("package: %s: %s%s%s", a->package.path, a->package.found ? "found" : "not installed",
                 a->package.problem[0] ? ", " : "", a->package.problem);
      break;
   case AKENO_CMD_SAVE_SETTINGS:
      apply_settings(a);
      toast(a, now, akeno_settings_save(&a->settings, a->storage) ? "Settings saved"
                                                                   : "Could not save settings");
      break;
   case AKENO_CMD_EXIT: return false;
   }
   return true;
}

int
akeno_app_run(const akeno_app_options *opt)
{
   static app a;
   memset(&a, 0, sizeof(a));
   if (akeno_platform_init("AkenoBlox") != 0)
      return 2;
   a.storage = akeno_platform_storage_root();
   akeno_log_open_file(a.storage);
   AKENO_INFO("%s starting", AKENO_VERSION_STRING);
   AKENO_INFO("storage: %s", a.storage);

   akeno_settings_defaults(&a.settings);
   if (!akeno_settings_load(&a.settings, a.storage))
      AKENO_INFO("settings: none saved yet, defaults in use");
   apply_settings(&a);
   akeno_gamepad_mapping_default(&a.mapping);
   a.storage_check = -1;
   a.network_check = -1;
   a.dns_control_check = -1;
   a.udp_check = -1;
   akeno_menu_init(&a.menu, &a.settings, akeno_platform_now_us());
   akeno_input_tracker_reset(&a.tracker);
   rebuild_report(&a);

   int status = 0;
   if (opt->run_probes_at_start) {
      run_probes(&a);
      if (!a.selftest.passed)
         status = 1;
   }

   char err[160];
   a.frame.width = AKENO_UI_WIDTH;
   a.frame.height = AKENO_UI_HEIGHT;
   a.frame.stride = AKENO_UI_WIDTH;
   a.frame.pixels = malloc((size_t)AKENO_UI_WIDTH * AKENO_UI_HEIGHT * sizeof(uint32_t));
   if (a.frame.pixels == NULL) {
      AKENO_ERROR("no memory for the frame");
      akeno_platform_shutdown();
      return 2;
   }
   if (akeno_platform_display_open(AKENO_UI_WIDTH, AKENO_UI_HEIGHT, err, sizeof(err)) != 0) {
      AKENO_ERROR("display: %s", err);
      free(a.frame.pixels);
      akeno_log_close();
      akeno_platform_shutdown();
      return 2;
   }
   AKENO_INFO("display: %ux%u", AKENO_UI_WIDTH, AKENO_UI_HEIGHT);
   a.display_open = true;
   rebuild_report(&a);

   bool running = true, dirty = true;
   uint64_t frames = 0, script_done_frames = 0;
   akeno_pad_state pad;
   akeno_android_gamepad gamepad;
   bool had_pad = false;
   while (running) {
      const uint64_t now = akeno_platform_now_us();
      memset(&pad, 0, sizeof(pad));
      const bool have_pad = akeno_platform_pad_poll(&pad);
      if (have_pad != had_pad) {
         AKENO_INFO("controller %s", have_pad ? "connected" : "not connected");
         had_pad = have_pad;
         dirty = true;
         if (have_pad && !a.pad_connected) {
            a.pad_connected = true;
            rebuild_report(&a);
         }
      }
      bool changed;
      const akeno_command command = step_menu(&a, &pad, now, &changed);
      dirty |= changed;
      if (command != AKENO_CMD_NONE)
         AKENO_DEBUG("command %d", (int)command);
      running = execute(&a, command, now);
      if (a.toast[0] && now >= a.toast_until_us) {
         a.toast[0] = '\0';
         dirty = true;
      }
      akeno_android_gamepad_from_pad(&pad, &a.mapping, a.settings.stick_deadzone, &gamepad);

      if (dirty) {
         const akeno_view view = {
            .menu = &a.menu,
            .platform = &a.platform,
            .report = &a.report,
            .probes_ran = a.probes_ran,
            .selftest_passed = a.selftest.passed,
            .package = &a.package,
            .pad = &pad,
            .gamepad = &gamepad,
            .mapping = &a.mapping,
            .now_us = now,
            .toast = a.toast[0] ? a.toast : NULL,
            .storage_root = a.storage,
         };
         akeno_screens_draw(&a.frame, &view, &a.menu.list_rows, &a.menu.visible_rows);
         a.frame.serial++;
         dirty = false;
      }
      if (akeno_platform_display_present(&a.frame) != 0) {
         AKENO_ERROR("display: present failed");
         status = status ? status : 3;
         break;
      }
      frames++;
      if (opt->max_frames != 0 && frames >= opt->max_frames)
         break;
#if !defined(AKENO_PS5)
      if (opt->stop_when_script_done && akeno_host_input_script_done() && ++script_done_frames > 3)
         break;
      if (!opt->fast)
         akeno_platform_sleep_us(16000);
#else
      (void)script_done_frames;
#endif
   }
   AKENO_INFO("leaving after %llu frames", (unsigned long long)frames);

#if !defined(AKENO_PS5)
   if (opt->screenshot_path != NULL && akeno_host_save_last_frame(opt->screenshot_path) != 0)
      AKENO_WARN("could not write %s", opt->screenshot_path);
#endif
   if (opt->report_path != NULL && akeno_report_write(&a.report, opt->report_path) != 0)
      AKENO_WARN("could not write %s", opt->report_path);
   if (a.menu.settings_dirty)
      akeno_settings_save(&a.settings, a.storage);
   akeno_platform_display_close();
   free(a.frame.pixels);
   akeno_log_close();
   akeno_platform_shutdown();
   return status;
}
