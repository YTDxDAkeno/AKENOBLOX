/*
 * AkenoBlox PS5 - the log: one line at a time to the platform's log, a file
 * under storage, and a ring the Diagnostics screen shows.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Nothing that could be a credential is written anywhere: every line passes
 * through akeno_log_redact first, which masks Roblox session cookies and the
 * usual shapes of tokens and passwords (docs/SECURITY.md).
 */
#ifndef AKENO_LOG_H
#define AKENO_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum akeno_log_level {
   AKENO_LOG_DEBUG = 0,
   AKENO_LOG_INFO,
   AKENO_LOG_WARN,
   AKENO_LOG_ERROR,
} akeno_log_level;

enum { AKENO_LOG_LINE_BYTES = 160, AKENO_LOG_RING_LINES = 64 };

/* Opens <storage>/logs/akenoblox.log (the previous run's is kept as .1).
 * Logging works before and without it; then lines only reach the platform. */
void akeno_log_open_file(const char *storage_root);
void akeno_log_close(void);
void akeno_log_set_level(akeno_log_level level);

void akeno_log(akeno_log_level level, const char *format, ...) __attribute__((format(printf, 2, 3)));
#define AKENO_DEBUG(...) akeno_log(AKENO_LOG_DEBUG, __VA_ARGS__)
#define AKENO_INFO(...) akeno_log(AKENO_LOG_INFO, __VA_ARGS__)
#define AKENO_WARN(...) akeno_log(AKENO_LOG_WARN, __VA_ARGS__)
#define AKENO_ERROR(...) akeno_log(AKENO_LOG_ERROR, __VA_ARGS__)

/* Masks secrets in line, in place: the value after .ROBLOSECURITY,
 * "password", "token", "cookie", "authorization" and "secret" keys (=, : or
 * whitespace separated) becomes "[redacted]". Returns the number masked. */
int akeno_log_redact(char *line, size_t size);

/* The ring, oldest first: count lines written into lines[]. */
size_t akeno_log_recent(char lines[][AKENO_LOG_LINE_BYTES], size_t max);
/* Lines logged at WARN or above since start. */
uint32_t akeno_log_warning_count(void);

#ifdef __cplusplus
}
#endif

#endif /* AKENO_LOG_H */
