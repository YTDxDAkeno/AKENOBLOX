/*
 * AkenoBlox PS5 - the platform on a POSIX host.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Development and CI only: input comes from a script, frames go to a buffer
 * that can be saved as a PPM, and the log is standard error. The point is
 * that everything above this file runs here exactly as it runs on the
 * console, so the menu, the drawing and the loader are tested on every
 * commit without one.
 */
#define _DEFAULT_SOURCE
#include "akeno_host.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>

static struct timespec start_time;
static char storage_root[512] = "";

/* ---------------------------------------------------------- lifecycle */

int
akeno_platform_init(const char *title)
{
   (void)title;
   clock_gettime(CLOCK_MONOTONIC, &start_time);
   return 0;
}

void
akeno_platform_shutdown(void)
{
   akeno_platform_display_close();
}

_Noreturn void
akeno_platform_exit(int status)
{
   fflush(NULL);
   exit(status);
}

/* -------------------------------------------------------------- facts */

void
akeno_platform_query(akeno_platform_info *info)
{
   memset(info, 0, sizeof(*info));
   snprintf(info->platform, sizeof(info->platform), "linux-host");
   struct utsname name;
   if (uname(&name) == 0)
      snprintf(info->firmware, sizeof(info->firmware), "%.31s", name.release);
   else
      snprintf(info->firmware, sizeof(info->firmware), "unknown");
   info->page_size = akeno_host_code_memory()->page_size;
   FILE *meminfo = fopen("/proc/meminfo", "r");
   if (meminfo != NULL) {
      char line[128];
      unsigned long long kib;
      while (fgets(line, sizeof(line), meminfo) != NULL) {
         if (sscanf(line, "MemTotal: %llu kB", &kib) == 1)
            info->memory_total = kib * 1024u;
         else if (sscanf(line, "MemAvailable: %llu kB", &kib) == 1)
            info->memory_free = kib * 1024u;
      }
      fclose(meminfo);
      info->memory_known = info->memory_total != 0;
   }
}

const akeno_mem_ops *
akeno_platform_code_memory(void)
{
   return akeno_host_code_memory();
}

/* --------------------------------------------------------------- time */

uint64_t
akeno_platform_now_us(void)
{
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   const int64_t seconds = now.tv_sec - start_time.tv_sec;
   const int64_t nanos = now.tv_nsec - start_time.tv_nsec;
   const int64_t micros = seconds * 1000000 + nanos / 1000;
   return micros > 0 ? (uint64_t)micros : 0;
}

void
akeno_platform_sleep_us(uint32_t microseconds)
{
   struct timespec t = {(time_t)(microseconds / 1000000u), (long)(microseconds % 1000000u) * 1000L};
   while (nanosleep(&t, &t) != 0 && errno == EINTR) {
   }
}

/* ------------------------------------------------------------ logging */

void
akeno_platform_log_line(const char *line)
{
   fprintf(stderr, "[AkenoBlox] %s\n", line);
}

/* ------------------------------------------------------------ storage */

void
akeno_host_set_storage_root(const char *path)
{
   snprintf(storage_root, sizeof(storage_root), "%s", path);
}

const char *
akeno_platform_storage_root(void)
{
   if (storage_root[0] == '\0') {
      const char *env = getenv("AKENO_DATA_DIR");
      snprintf(storage_root, sizeof(storage_root), "%s", env && env[0] ? env : "./akeno-data");
   }
   if (mkdir(storage_root, 0777) != 0 && errno != EEXIST)
      fprintf(stderr, "[AkenoBlox] cannot create %s: %s\n", storage_root, strerror(errno));
   return storage_root;
}

const char *
akeno_platform_app_root(void)
{
   return ".";
}

/* -------------------------------------------------------------- input */

typedef struct script_step {
   uint32_t buttons;
   int frames;  /* polls the step lasts */
   bool release_after;
} script_step;

enum { SCRIPT_MAX = 512 };
static script_step script[SCRIPT_MAX];
static int script_length, script_index, script_frame;
static bool script_set, script_releasing;

static const char *const script_buttons[AKENO_BTN_COUNT] = {
   "up", "down", "left", "right", "cross", "circle", "square", "triangle",
   "l1", "r1",   "l2",   "r2",    "l3",    "r3",     "options", "touchpad",
};

static int
button_bit(const char *name, size_t length)
{
   for (int i = 0; i < AKENO_BTN_COUNT; i++)
      if (strlen(script_buttons[i]) == length && strncmp(script_buttons[i], name, length) == 0)
         return i;
   return -1;
}

bool
akeno_host_set_input_script(const char *text)
{
   script_length = script_index = script_frame = 0;
   script_releasing = false;
   script_set = true;
   const char *at = text;
   while (at != NULL && *at) {
      const char *comma = strchr(at, ',');
      const size_t length = comma ? (size_t)(comma - at) : strlen(at);
      char word[64];
      if (length == 0 || length >= sizeof(word) || script_length == SCRIPT_MAX)
         return false;
      memcpy(word, at, length);
      word[length] = '\0';
      script_step step = {0, 1, true};
      if (strcmp(word, "wait") == 0) {
         step.release_after = false;
      } else if (strncmp(word, "wait:", 5) == 0) {
         step.frames = atoi(word + 5);
         step.release_after = false;
         if (step.frames <= 0)
            return false;
      } else if (strncmp(word, "hold:", 5) == 0) {
         /* hold:<button>:<polls> */
         const char *name = word + 5;
         const char *colon = strchr(name, ':');
         const int bit = colon ? button_bit(name, (size_t)(colon - name)) : -1;
         if (bit < 0 || atoi(colon + 1) <= 0)
            return false;
         step.buttons = 1u << bit;
         step.frames = atoi(colon + 1);
      } else {
         const int bit = button_bit(word, length);
         if (bit < 0)
            return false;
         step.buttons = 1u << bit;
      }
      script[script_length++] = step;
      at = comma ? comma + 1 : NULL;
   }
   return true;
}

bool
akeno_host_input_script_done(void)
{
   return script_set && script_index >= script_length;
}

bool
akeno_platform_pad_poll(akeno_pad_state *state)
{
   memset(state, 0, sizeof(*state));
   if (!script_set)
      return false;
   state->connected = true;
   state->timestamp_us = akeno_platform_now_us();
   if (script_index >= script_length)
      return true;
   const script_step *step = &script[script_index];
   if (script_releasing) {
      /* One released poll after a press, so the next press is an edge. */
      script_releasing = false;
      script_index++;
      script_frame = 0;
      return true;
   }
   state->buttons = step->buttons;
   if (++script_frame >= step->frames) {
      script_frame = 0;
      if (step->release_after)
         script_releasing = true;
      else
         script_index++;
   }
   return true;
}

/* ------------------------------------------------------------ display */

static uint32_t *last_frame;
static unsigned last_width, last_height;
static uint64_t presented;

int
akeno_platform_display_open(unsigned width, unsigned height, char *err, size_t err_size)
{
   free(last_frame);
   last_frame = calloc((size_t)width * height, sizeof(uint32_t));
   if (last_frame == NULL) {
      snprintf(err, err_size, "no memory for a %ux%u frame", width, height);
      return -1;
   }
   last_width = width;
   last_height = height;
   presented = 0;
   return 0;
}

int
akeno_platform_display_present(const akeno_frame *frame)
{
   if (last_frame == NULL || frame == NULL || frame->width != last_width ||
       frame->height != last_height)
      return -1;
   for (unsigned y = 0; y < frame->height; y++)
      memcpy(last_frame + (size_t)y * last_width, frame->pixels + (size_t)y * frame->stride,
             (size_t)frame->width * sizeof(uint32_t));
   presented++;
   return 0;
}

void
akeno_platform_display_close(void)
{
   free(last_frame);
   last_frame = NULL;
}

uint64_t
akeno_host_presented_frames(void)
{
   return presented;
}

int
akeno_host_save_last_frame(const char *path)
{
   if (last_frame == NULL)
      return -1;
   FILE *f = fopen(path, "wb");
   if (f == NULL)
      return -1;
   fprintf(f, "P6\n%u %u\n255\n", last_width, last_height);
   uint8_t *row = malloc((size_t)last_width * 3);
   if (row == NULL) {
      fclose(f);
      return -1;
   }
   for (unsigned y = 0; y < last_height; y++) {
      for (unsigned x = 0; x < last_width; x++) {
         const uint32_t p = last_frame[(size_t)y * last_width + x];
         row[x * 3 + 0] = (uint8_t)(p >> 16);
         row[x * 3 + 1] = (uint8_t)(p >> 8);
         row[x * 3 + 2] = (uint8_t)p;
      }
      fwrite(row, 1, (size_t)last_width * 3, f);
   }
   free(row);
   return fclose(f) == 0 ? 0 : -1;
}

/* ------------------------------------------------------------ network */

int
akeno_platform_network_check(const char *host, char *summary, size_t summary_size)
{
   struct addrinfo hints = {0}, *result = NULL;
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   const int rc = getaddrinfo(host, "443", &hints, &result);
   if (rc != 0) {
      snprintf(summary, summary_size, "%s does not resolve: %s", host, gai_strerror(rc));
      return -1;
   }
   int count = 0;
   for (struct addrinfo *a = result; a != NULL; a = a->ai_next)
      count++;
   freeaddrinfo(result);
   snprintf(summary, summary_size, "%s resolves (%d addresses)", host, count);
   return 0;
}
