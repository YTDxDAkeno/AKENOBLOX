/*
 * AkenoBlox PS5 - the self-test, run on the host: what the console will run.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_test.h"

#include "akeno_host.h"
#include "diagnostics/akeno_cpu.h"
#include "diagnostics/akeno_report.h"
#include "diagnostics/akeno_selftest.h"

static void
run_with(const akeno_mem_ops *mem)
{
   akeno_selftest_result r;
   akeno_selftest_run(mem, true, &r);
   CHECK(r.ran);
   for (int i = 0; i < r.step_count; i++)
      if (!r.steps[i].passed)
         fprintf(stderr, "  step %s: %s\n", r.steps[i].name, r.steps[i].detail);
   CHECK(r.passed);
   CHECK(r.step_count >= 12);
   CHECK_EQ_INT(r.failed_count, 0);
   CHECK_EQ_INT((int)strlen(r.fixture_sha256), 64);
}

static void test_selftest_kernel_pages(void) { run_with(akeno_host_code_memory()); }
static void test_selftest_console_pages(void) { run_with(akeno_host_code_memory_paged(16384)); }

static void
test_selftest_without_memory(void)
{
   akeno_selftest_result r;
   akeno_selftest_run(NULL, false, &r);
   CHECK(r.ran && !r.passed && r.failed_count == 1);
}

static void
test_cpu_levels(void)
{
   const uint32_t v2 = AKENO_CPU_CX16 | AKENO_CPU_LAHF | AKENO_CPU_POPCNT | AKENO_CPU_SSE3 |
                       AKENO_CPU_SSSE3 | AKENO_CPU_SSE41 | AKENO_CPU_SSE42;
   const uint32_t v3 = v2 | AKENO_CPU_AVX | AKENO_CPU_AVX2 | AKENO_CPU_BMI1 | AKENO_CPU_BMI2 |
                       AKENO_CPU_F16C | AKENO_CPU_FMA | AKENO_CPU_LZCNT | AKENO_CPU_MOVBE |
                       AKENO_CPU_OSXSAVE;
   CHECK_EQ_INT(akeno_cpu_level(0, false), 1);
   CHECK_EQ_INT(akeno_cpu_level(v2 & ~AKENO_CPU_SSE42, false), 1);
   CHECK_EQ_INT(akeno_cpu_level(v2, false), 2);
   CHECK_EQ_INT(akeno_cpu_level(v3, false), 2); /* AVX the OS does not save is not v3 */
   CHECK_EQ_INT(akeno_cpu_level(v3, true), 3);
   CHECK_EQ_INT(akeno_cpu_level(v3 | AKENO_CPU_AVX512F, true), 4);
   akeno_cpu_info host;
   akeno_cpu_query(&host);
   CHECK(host.available);
   CHECK(host.vendor[0] != '\0');
   CHECK(akeno_cpu_meets_android_floor(&host)); /* any CI runner is v2 or better */
}

static void
test_fs_probe_on_host(void)
{
   akeno_fs_probe p;
   akeno_fs_probe_run(&p);
   CHECK(p.ran && p.thread_ran);
   CHECK(p.main_stable && p.thread_stable); /* glibc's own stack guard lives there */
}

static void
test_report(void)
{
   akeno_report r;
   akeno_report_clear(&r);
   akeno_report_build(&r);
   akeno_platform_info info;
   akeno_platform_query(&info);
   akeno_report_system(&r, &info);
   akeno_cpu_info cpu = {0};
   akeno_report_cpu(&r, &cpu);
   CHECK(r.rows[r.count - 1].status == AKENO_ROW_PENDING); /* not run is pending, not ok */
   for (int i = 0; i < AKENO_REPORT_MAX_ROWS + 10; i++)
      akeno_report_add(&r, "Flood", AKENO_ROW_INFO, "row", "%d", i);
   CHECK_EQ_INT(r.count, AKENO_REPORT_MAX_ROWS);
   CHECK(r.dropped > 0);
   char path[] = "/tmp/akeno-report-test.txt";
   CHECK_EQ_INT(akeno_report_write(&r, path), 0);
   remove(path);
}

static const akeno_test_case cases[] = {
   {"selftest_kernel_pages", test_selftest_kernel_pages},
   {"selftest_console_pages", test_selftest_console_pages},
   {"selftest_without_memory", test_selftest_without_memory},
   {"cpu_levels", test_cpu_levels},
   {"fs_probe_on_host", test_fs_probe_on_host},
   {"report", test_report},
};

AKENO_TEST_MAIN(cases)
