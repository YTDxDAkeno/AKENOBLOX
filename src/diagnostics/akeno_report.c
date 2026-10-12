/*
 * AkenoBlox PS5 - the diagnostics report.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_report.h"

#include "akeno_version.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *const status_names[] = {"info", "ok", "warn", "FAIL", "pending"};

const char *
akeno_row_status_name(akeno_row_status status)
{
   return (unsigned)status <= AKENO_ROW_PENDING ? status_names[status] : "?";
}

void
akeno_report_clear(akeno_report *report)
{
   memset(report, 0, sizeof(*report));
}

void
akeno_report_add(akeno_report *report, const char *section, akeno_row_status status,
                 const char *label, const char *format, ...)
{
   if (report->count >= AKENO_REPORT_MAX_ROWS) {
      report->dropped++;
      return;
   }
   akeno_report_row *row = &report->rows[report->count++];
   snprintf(row->section, sizeof(row->section), "%s", section);
   snprintf(row->label, sizeof(row->label), "%s", label);
   row->status = status;
   va_list args;
   va_start(args, format);
   vsnprintf(row->value, sizeof(row->value), format, args);
   va_end(args);
}

void
akeno_report_build(akeno_report *r)
{
   akeno_report_add(r, "Build", AKENO_ROW_INFO, "Version", "%s", AKENO_VERSION);
   akeno_report_add(r, "Build", AKENO_ROW_INFO, "Commit", "%s", AKENO_COMMIT);
#if defined(AKENO_PS5)
   akeno_report_add(r, "Build", AKENO_ROW_INFO, "Target", "x86_64-sie-ps5 (payload SDK v0.42)");
#else
   akeno_report_add(r, "Build", AKENO_ROW_INFO, "Target", "host (development build)");
#endif
}

static void
mib(char *out, size_t size, uint64_t bytes)
{
   snprintf(out, size, "%llu MiB", (unsigned long long)(bytes >> 20));
}

void
akeno_report_system(akeno_report *r, const akeno_platform_info *info)
{
   akeno_report_add(r, "System", AKENO_ROW_INFO, "Platform", "%s", info->platform);
   akeno_report_add(r, "System", info->firmware[0] && strcmp(info->firmware, "unknown") != 0
                                    ? AKENO_ROW_INFO
                                    : AKENO_ROW_WARN,
                    "Firmware", "%s", info->firmware);
   akeno_report_add(r, "System", AKENO_ROW_INFO, "Code page size", "%zu bytes", info->page_size);
   if (!info->memory_known) {
      akeno_report_add(r, "System", AKENO_ROW_WARN, "Memory", "not available");
      return;
   }
   char total[32], free_[32];
   mib(total, sizeof(total), info->memory_total);
   mib(free_, sizeof(free_), info->memory_free);
   akeno_report_add(r, "System", AKENO_ROW_INFO, "Memory", "%s free of %s", free_, total);
   if (info->memory_largest) {
      mib(free_, sizeof(free_), info->memory_largest);
      akeno_report_add(r, "System", AKENO_ROW_INFO, "Largest free range", "%s", free_);
   }
   if (info->flexible_free) {
      mib(free_, sizeof(free_), info->flexible_free);
      akeno_report_add(r, "System", AKENO_ROW_INFO, "Flexible memory free", "%s", free_);
   }
}

void
akeno_report_cpu(akeno_report *r, const akeno_cpu_info *c)
{
   if (!c->available) {
      akeno_report_add(r, "CPU", AKENO_ROW_PENDING, "CPUID", "not run");
      return;
   }
   akeno_report_add(r, "CPU", AKENO_ROW_INFO, "Processor", "%s", c->brand[0] ? c->brand : c->vendor);
   akeno_report_add(r, "CPU", AKENO_ROW_INFO, "Family/model", "%u / %u, stepping %u", c->family,
                    c->model, c->stepping);
   akeno_report_add(r, "CPU", akeno_cpu_meets_android_floor(c) ? AKENO_ROW_OK : AKENO_ROW_FAIL,
                    "x86-64 level", "v%d (Roblox's Android build needs v2)", c->level);
   const uint32_t f = c->features;
   akeno_report_add(r, "CPU", AKENO_ROW_INFO, "Features", "%s%s%s%s%s%s%s%s%s",
                    f & AKENO_CPU_SSE42 ? "SSE4.2 " : "", f & AKENO_CPU_POPCNT ? "POPCNT " : "",
                    f & AKENO_CPU_AVX ? "AVX " : "", f & AKENO_CPU_AVX2 ? "AVX2 " : "",
                    f & AKENO_CPU_BMI1 ? "BMI1 " : "", f & AKENO_CPU_BMI2 ? "BMI2 " : "",
                    f & AKENO_CPU_MOVBE ? "MOVBE " : "", f & AKENO_CPU_LZCNT ? "LZCNT " : "",
                    f & AKENO_CPU_FMA ? "FMA" : "");
   akeno_report_add(r, "CPU", c->ymm_enabled ? AKENO_ROW_OK : AKENO_ROW_WARN, "AVX state saved",
                    "%s", c->ymm_enabled ? "yes (XCR0)" : "no");
}

void
akeno_report_fs_probe(akeno_report *r, const akeno_fs_probe *p)
{
   if (!p->ran) {
      akeno_report_add(r, "Android ABI", AKENO_ROW_PENDING, "%fs:0x28 canary word", "not run");
      return;
   }
   /* The stack canary is process-sensitive information. Its actual value
    * must not end up in a world-readable report uploaded for debugging. */
   akeno_report_add(r, "Android ABI", p->main_stable ? AKENO_ROW_OK : AKENO_ROW_FAIL,
                    "%fs:0x28, main thread", "%s (canary value redacted)",
                    p->main_stable ? "stable" : "CHANGED");
   if (!p->thread_ran) {
      akeno_report_add(r, "Android ABI", AKENO_ROW_WARN, "%fs:0x28, new thread",
                       "pthread_create failed");
      return;
   }
   akeno_report_add(r, "Android ABI", p->thread_stable ? AKENO_ROW_OK : AKENO_ROW_FAIL,
                    "%fs:0x28, new thread", "%s (%s the main thread's)",
                    p->thread_stable ? "stable" : "CHANGED",
                    p->differs_between_threads ? "differs from" : "same as");
}

void
akeno_report_selftest(akeno_report *r, const akeno_selftest_result *t)
{
   if (!t->ran) {
      akeno_report_add(r, "ELF loader", AKENO_ROW_PENDING, "Self-test", "not run");
      return;
   }
   akeno_report_add(r, "ELF loader", t->passed ? AKENO_ROW_OK : AKENO_ROW_FAIL, "Self-test",
                    "%s: %d of %d steps passed in %llu us", t->passed ? "PASSED" : "FAILED",
                    t->step_count - t->failed_count, t->step_count,
                    (unsigned long long)t->duration_us);
   for (int i = 0; i < t->step_count; i++)
      akeno_report_add(r, "ELF loader", t->steps[i].passed ? AKENO_ROW_OK : AKENO_ROW_FAIL,
                       t->steps[i].name, "%s", t->steps[i].detail);
   akeno_report_add(r, "ELF loader", AKENO_ROW_INFO, "Fixture sha256", "%.16s...",
                    t->fixture_sha256);
}

int
akeno_report_write(const akeno_report *report, const char *path)
{
   FILE *f = fopen(path, "w");
   if (f == NULL)
      return -1;
   fprintf(f, "%s\n\n", AKENO_VERSION_STRING);
   const char *section = "";
   for (int i = 0; i < report->count; i++) {
      const akeno_report_row *row = &report->rows[i];
      if (strcmp(row->section, section) != 0) {
         fprintf(f, "%s[%s]\n", i ? "\n" : "", row->section);
         section = row->section;
      }
      fprintf(f, "  %-7s %-36s %s\n", akeno_row_status_name(row->status), row->label, row->value);
   }
   if (report->dropped)
      fprintf(f, "\n(%d rows did not fit)\n", report->dropped);
   const bool ok = fclose(f) == 0;
   chmod(path, 0666);
   return ok ? 0 : -1;
}
