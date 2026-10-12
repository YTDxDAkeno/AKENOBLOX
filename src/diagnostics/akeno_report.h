/*
 * AkenoBlox PS5 - a diagnostics report: rows of label, value and status,
 * shown on the Diagnostics screen and exported as text for an issue.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A row is only marked OK when something was measured to be OK. What was not
 * run is PENDING, never OK (docs/TEST_REPORTS.md's vocabulary).
 */
#ifndef AKENO_REPORT_H
#define AKENO_REPORT_H

#include <stdbool.h>
#include <stddef.h>

#include "akeno_platform.h"
#include "diagnostics/akeno_cpu.h"
#include "diagnostics/akeno_selftest.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum akeno_row_status {
   AKENO_ROW_INFO = 0,
   AKENO_ROW_OK,
   AKENO_ROW_WARN,
   AKENO_ROW_FAIL,
   AKENO_ROW_PENDING,
} akeno_row_status;

enum { AKENO_REPORT_MAX_ROWS = 96 };

typedef struct akeno_report_row {
   char section[24];
   char label[40];
   char value[104];
   akeno_row_status status;
} akeno_report_row;

typedef struct akeno_report {
   akeno_report_row rows[AKENO_REPORT_MAX_ROWS];
   int count;
   int dropped; /* rows that did not fit */
} akeno_report;

const char *akeno_row_status_name(akeno_row_status status);
void akeno_report_clear(akeno_report *report);
void akeno_report_add(akeno_report *report, const char *section, akeno_row_status status,
                      const char *label, const char *format, ...)
   __attribute__((format(printf, 5, 6)));

/* The sections. */
void akeno_report_build(akeno_report *report);  /* version and build */
void akeno_report_system(akeno_report *report, const akeno_platform_info *info);
void akeno_report_cpu(akeno_report *report, const akeno_cpu_info *cpu);
void akeno_report_fs_probe(akeno_report *report, const akeno_fs_probe *probe);
void akeno_report_selftest(akeno_report *report, const akeno_selftest_result *result);

/* Writes the report as text to path; 0 on success. */
int akeno_report_write(const akeno_report *report, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_REPORT_H */
