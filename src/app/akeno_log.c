/*
 * AkenoBlox PS5 - the log.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_log.h"

#include "akeno_platform.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *log_file;
static akeno_log_level log_threshold = AKENO_LOG_INFO;
static char ring[AKENO_LOG_RING_LINES][AKENO_LOG_LINE_BYTES];
static size_t ring_next, ring_count;
static uint32_t warnings;

static const char *const level_names[] = {"debug", "info", "warn", "error"};

void
akeno_log_set_level(akeno_log_level level)
{
   log_threshold = level;
}

void
akeno_log_open_file(const char *storage_root)
{
   if (storage_root == NULL || storage_root[0] == '\0')
      return;
   char directory[512], path[560], previous[580];
   snprintf(directory, sizeof(directory), "%s/logs", storage_root);
   if (mkdir(directory, 0777) != 0 && errno != EEXIST)
      return;
   /* Everything a title creates must stay readable over FTP, whatever the
    * umask (PS5_VulkanTemplate platform-contracts.md, "Files and folders"). */
   chmod(directory, 0777);
   snprintf(path, sizeof(path), "%s/akenoblox.log", directory);
   snprintf(previous, sizeof(previous), "%s/akenoblox.log.1", directory);
   rename(path, previous);
   pthread_mutex_lock(&log_lock);
   if (log_file == NULL) {
      log_file = fopen(path, "w");
      if (log_file != NULL)
         chmod(path, 0666);
   }
   pthread_mutex_unlock(&log_lock);
}

void
akeno_log_close(void)
{
   pthread_mutex_lock(&log_lock);
   if (log_file != NULL) {
      fclose(log_file);
      log_file = NULL;
   }
   pthread_mutex_unlock(&log_lock);
}

/* Header-shaped keys (Authorization: Bearer <token>, Cookie: a=b; c=d) carry
 * their secret after a first word, so everything to the end of the line goes. */
static const struct {
   const char *key;
   bool whole_line;
} secret_keys[] = {
   {"authorization", true}, {"cookie", true},  {".roblosecurity", false}, {"password", false},
   {"passwd", false},       {"token", false},  {"secret", false},         {"session", false},
};

/* Case-insensitive prefix match of key at s. */
static bool
key_at(const char *s, const char *key)
{
   for (; *key; s++, key++)
      if (tolower((unsigned char)*s) != *key)
         return false;
   return true;
}

static bool
value_end(char c)
{
   return c == '\0' || c == ';' || c == ',' || c == '&' || c == '"' || c == '\'' ||
          isspace((unsigned char)c);
}

int
akeno_log_redact(char *line, size_t size)
{
   if (line == NULL || size == 0)
      return 0;
   char out[1024];
   size_t o = 0;
   int masked = 0;
   const char *s = line;
   while (*s && o + 1 < sizeof(out)) {
      size_t key_length = 0;
      bool whole_line = false;
      for (size_t k = 0; k < sizeof(secret_keys) / sizeof(secret_keys[0]); k++)
         if (key_at(s, secret_keys[k].key)) {
            key_length = strlen(secret_keys[k].key);
            whole_line = secret_keys[k].whole_line;
            break;
         }
      if (key_length == 0) {
         out[o++] = *s++;
         continue;
      }
      /* Copy the key and any letters that continue it ("tokens", "cookies"). */
      while (key_length-- > 0 && o + 1 < sizeof(out))
         out[o++] = *s++;
      while ((isalnum((unsigned char)*s) || *s == '_') && o + 1 < sizeof(out))
         out[o++] = *s++;
      /* An = or : must follow (quotes and spaces around it allowed), or this
       * was a word in a sentence, not a key. */
      const char *value = s;
      size_t separators = 0;
      bool assigns = false;
      while ((*value == '=' || *value == ':' || *value == ' ' || *value == '"' || *value == '\'' ||
              *value == '\t') &&
             separators < 4) {
         assigns |= *value == '=' || *value == ':';
         value++;
         separators++;
      }
      if (!assigns || value_end(*value))
         continue;
      while (s < value && o + 1 < sizeof(out))
         out[o++] = *s++;
      while (whole_line ? (*s != '\0' && *s != '\n') : !value_end(*s))
         s++;
      const char *mask = "[redacted]";
      while (*mask && o + 1 < sizeof(out))
         out[o++] = *mask++;
      masked++;
   }
   out[o] = '\0';
   snprintf(line, size, "%s", out);
   return masked;
}

void
akeno_log(akeno_log_level level, const char *format, ...)
{
   if (level < log_threshold || (unsigned)level > AKENO_LOG_ERROR)
      return;
   char message[AKENO_LOG_LINE_BYTES * 2];
   va_list args;
   va_start(args, format);
   vsnprintf(message, sizeof(message), format, args);
   va_end(args);
   akeno_log_redact(message, sizeof(message));

   char line[AKENO_LOG_LINE_BYTES * 2 + 32];
   const uint64_t now = akeno_platform_now_us();
   snprintf(line, sizeof(line), "%6llu.%03llu %-5s %s", (unsigned long long)(now / 1000000u),
            (unsigned long long)(now / 1000u % 1000u), level_names[level], message);

   pthread_mutex_lock(&log_lock);
   if (level >= AKENO_LOG_WARN)
      warnings++;
   snprintf(ring[ring_next], AKENO_LOG_LINE_BYTES, "%.*s", AKENO_LOG_LINE_BYTES - 1, line);
   ring_next = (ring_next + 1) % AKENO_LOG_RING_LINES;
   if (ring_count < AKENO_LOG_RING_LINES)
      ring_count++;
   if (log_file != NULL) {
      fputs(line, log_file);
      fputc('\n', log_file);
      fflush(log_file);
   }
   pthread_mutex_unlock(&log_lock);
   akeno_platform_log_line(line);
}

size_t
akeno_log_recent(char lines[][AKENO_LOG_LINE_BYTES], size_t max)
{
   pthread_mutex_lock(&log_lock);
   const size_t count = ring_count < max ? ring_count : max;
   const size_t first = (ring_next + AKENO_LOG_RING_LINES - count) % AKENO_LOG_RING_LINES;
   for (size_t i = 0; i < count; i++)
      memcpy(lines[i], ring[(first + i) % AKENO_LOG_RING_LINES], AKENO_LOG_LINE_BYTES);
   pthread_mutex_unlock(&log_lock);
   return count;
}

uint32_t
akeno_log_warning_count(void)
{
   return warnings;
}
