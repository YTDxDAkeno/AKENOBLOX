/*
 * AkenoBlox PS5 - the ELF loader's self-test.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "akeno_selftest.h"

#include "akeno_elf.h"
#include "akeno_platform.h"
#include "app/akeno_log.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* The embedded fixtures (cmake/AkenoFixtures.cmake, tools/build/embed.py). */
extern const unsigned char akeno_selftest_aps2[];
extern const size_t akeno_selftest_aps2_size;
extern const char akeno_selftest_aps2_sha256[];
extern const unsigned char akeno_selftest_relr[];
extern const size_t akeno_selftest_relr_size;
extern const unsigned char akeno_selftest_canary[];
extern const size_t akeno_selftest_canary_size;

/* ------------------------------------------- what the fixtures import */

static char events[64];

static int
selftest_host_add(int a, int b)
{
   return a + b + 1000;
}

static void
selftest_host_note(const char *event)
{
   const size_t used = strlen(events);
   snprintf(events + used, sizeof(events) - used, "%s;", event);
}

/* The canary fixture's __stack_chk_fail: the protected function found its
 * canary changed. Unwind back into the self-test rather than abort, so the
 * finding is reported instead of killing the program. */
static void *canary_jump[5];
static volatile int canary_armed;

static void
selftest_stack_chk_fail(void)
{
   if (canary_armed)
      __builtin_longjmp(canary_jump, 1);
   akeno_platform_log_line("selftest: __stack_chk_fail outside the canary step");
   akeno_platform_exit(70);
}

static akeno_symbol selftest_symbols[] = {
   {"akeno_host_add", (void *)selftest_host_add},
   {"akeno_host_note", (void *)selftest_host_note},
   {"__stack_chk_fail", (void *)selftest_stack_chk_fail},
};
static akeno_symbol_table selftest_table = {
   "libakeno_selftest.so", selftest_symbols, sizeof(selftest_symbols) / sizeof(selftest_symbols[0])};

static void *
resolve(void *ctx, const char *name, const char *version)
{
   (void)ctx;
   (void)version;
   return akeno_symbol_table_find(&selftest_table, name);
}

/* Answers everything but akeno_host_note: a library the runtime cannot fully
 * answer must be refused by name. */
static void *
resolve_without_note(void *ctx, const char *name, const char *version)
{
   if (strcmp(name, "akeno_host_note") == 0)
      return NULL;
   return resolve(ctx, name, version);
}

/* ------------------------------------------------------------- steps */

static akeno_selftest_step *
begin(akeno_selftest_result *r, const char *name)
{
   if (r->step_count >= AKENO_SELFTEST_MAX_STEPS)
      return NULL;
   akeno_selftest_step *s = &r->steps[r->step_count++];
   snprintf(s->name, sizeof(s->name), "%s", name);
   s->passed = false;
   s->detail[0] = '\0';
   AKENO_INFO("selftest: %s ...", name);
   return s;
}

static void
finish(akeno_selftest_result *r, akeno_selftest_step *s, bool passed, const char *format, ...)
   __attribute__((format(printf, 4, 5)));

static void
finish(akeno_selftest_result *r, akeno_selftest_step *s, bool passed, const char *format, ...)
{
   if (s == NULL)
      return;
   s->passed = passed;
   va_list args;
   va_start(args, format);
   vsnprintf(s->detail, sizeof(s->detail), format, args);
   va_end(args);
   if (!passed)
      r->failed_count++;
   AKENO_INFO("selftest: %s: %s (%s)", s->name, passed ? "pass" : "FAIL", s->detail);
}

typedef int (*int_void)(void);
typedef int (*int_int_int)(int, int);
typedef const char *(*str_void)(void);
typedef void *(*ptr_void)(void);
typedef void (*void_int_int)(int, int);

static void
exercise_aps2(const akeno_mem_ops *mem, akeno_selftest_result *r)
{
   akeno_selftest_step *s = begin(r, "parse the APS2 fixture");
   akeno_elf_info info;
   char why[160];
   const akeno_elf_status parsed =
      akeno_elf_parse(akeno_selftest_aps2, akeno_selftest_aps2_size, &info, why, sizeof(why));
   finish(r, s, parsed == AKENO_ELF_OK && info.relocs.android_rela_bytes > 0,
          "%s, %zu bytes, %u PT_LOAD, APS2 %llu bytes", akeno_elf_status_name(parsed),
          akeno_selftest_aps2_size, info.load_count,
          (unsigned long long)info.relocs.android_rela_bytes);
   if (parsed != AKENO_ELF_OK)
      return;

   s = begin(r, "copy, relocate, protect");
   events[0] = '\0';
   akeno_elf_load_options opt = {.mem = mem, .resolve = resolve, .run_constructors = true};
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   const akeno_elf_status loaded =
      akeno_elf_load(akeno_selftest_aps2, akeno_selftest_aps2_size, &opt, &m, &report);
   finish(r, s, loaded == AKENO_ELF_OK, "%s: %llu relocations, %u W+X pages, base %p%s%s",
          akeno_elf_status_name(loaded), (unsigned long long)report.relocations_applied,
          report.wx_pages, (void *)akeno_elf_base(m), report.message[0] ? ", " : "",
          report.message);
   if (loaded != AKENO_ELF_OK)
      return;

   s = begin(r, "constructor ran");
   int_void counter = (int_void)akeno_elf_symbol(m, "fixture_counter");
   const int count = counter ? counter() : -1;
   finish(r, s, count == 42 && strcmp(events, "init;") == 0, "counter %d, events \"%s\"", count,
          events);

   s = begin(r, "call into the host (PLT)");
   int_int_int add = (int_int_int)akeno_elf_symbol(m, "fixture_add");
   const int sum = add ? add(2, 3) : -1;
   finish(r, s, sum == 1005, "fixture_add(2, 3) = %d, expected 1005", sum);

   s = begin(r, "relative pointers");
   int_int_int apply = (int_int_int)akeno_elf_symbol(m, "fixture_apply");
   str_void greet = (str_void)akeno_elf_symbol(m, "fixture_greet");
   const int doubled = apply ? apply(0, 5) : -1, tripled = apply ? apply(1, 5) : -1;
   const char *greeting = greet ? greet() : NULL;
   finish(r, s, doubled == 10 && tripled == 15 && greeting && strcmp(greeting, "akeno") == 0,
          "apply %d %d, greeting \"%s\"", doubled, tripled, greeting ? greeting : "(null)");

   s = begin(r, "read and write data");
   int_void sum_data = (int_void)akeno_elf_symbol(m, "fixture_sum_data");
   void_int_int write = (void_int_int)akeno_elf_symbol(m, "fixture_write_data");
   const int before = sum_data ? sum_data() : -1;
   if (write)
      write(2, 100);
   const int after = sum_data ? sum_data() : -1;
   finish(r, s, before == 10 && after == 107, "sum %d then %d", before, after);

   s = begin(r, "import address (GLOB_DAT)");
   ptr_void address = (ptr_void)akeno_elf_symbol(m, "fixture_import_address");
   const void *seen = address ? address() : NULL;
   finish(r, s, seen == (void *)selftest_host_add, "%p, expected %p", seen,
          (void *)selftest_host_add);

   s = begin(r, "code pages executable");
   const unsigned code = counter ? akeno_elf_page_protection(m, (const void *)counter) : 0;
   finish(r, s, (code & AKENO_PROT_EXEC) != 0, "applied %s%s%s", code & AKENO_PROT_READ ? "r" : "-",
          code & AKENO_PROT_WRITE ? "w" : "-", code & AKENO_PROT_EXEC ? "x" : "-");

   s = begin(r, "unload runs the destructor");
   akeno_elf_unload(m);
   finish(r, s, strcmp(events, "init;fini;") == 0, "events \"%s\"", events);
}

static void
exercise_relr(const akeno_mem_ops *mem, akeno_selftest_result *r)
{
   akeno_selftest_step *s = begin(r, "RELR fixture");
   events[0] = '\0';
   akeno_elf_load_options opt = {.mem = mem, .resolve = resolve, .run_constructors = true};
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   const akeno_elf_status st =
      akeno_elf_load(akeno_selftest_relr, akeno_selftest_relr_size, &opt, &m, &report);
   int_int_int add = st == AKENO_ELF_OK ? (int_int_int)akeno_elf_symbol(m, "fixture_add") : NULL;
   const int sum = add ? add(40, 2) : -1;
   finish(r, s, st == AKENO_ELF_OK && sum == 1042, "%s, %llu relative, add = %d",
          akeno_elf_status_name(st), (unsigned long long)report.relative_applied, sum);
   akeno_elf_unload(m);

   s = begin(r, "missing import refused by name");
   opt.resolve = resolve_without_note;
   m = NULL;
   const akeno_elf_status missing =
      akeno_elf_load(akeno_selftest_relr, akeno_selftest_relr_size, &opt, &m, &report);
   finish(r, s,
          missing == AKENO_ELF_E_UNDEFINED_SYMBOL && m == NULL && report.missing_count == 1 &&
             strcmp(report.missing[0], "akeno_host_note") == 0,
          "%s, %u missing: %s", akeno_elf_status_name(missing), report.missing_count,
          report.missing_count ? report.missing[0] : "-");
   akeno_elf_unload(m);

   s = begin(r, "truncated image refused");
   opt.resolve = resolve;
   m = NULL;
   const akeno_elf_status cut = akeno_elf_load(akeno_selftest_relr, 200, &opt, &m, &report);
   finish(r, s, cut != AKENO_ELF_OK && m == NULL, "%s", akeno_elf_status_name(cut));
   akeno_elf_unload(m);
}

static void
exercise_canary(const akeno_mem_ops *mem, akeno_selftest_result *r)
{
   akeno_selftest_step *s = begin(r, "Android stack protector (%fs:0x28)");
   events[0] = '\0';
   akeno_elf_load_options opt = {.mem = mem, .resolve = resolve, .run_constructors = true};
   akeno_elf_module *m = NULL;
   const akeno_elf_status st =
      akeno_elf_load(akeno_selftest_canary, akeno_selftest_canary_size, &opt, &m, NULL);
   if (st != AKENO_ELF_OK) {
      finish(r, s, false, "load: %s", akeno_elf_status_name(st));
      return;
   }
   int (*protected_sum)(int) = (int (*)(int))akeno_elf_symbol(m, "canary_protected_sum");
   volatile int result = -1;
   canary_armed = 1;
   if (__builtin_setjmp(canary_jump) == 0) {
      result = protected_sum ? protected_sum(1) : -2;
      canary_armed = 0;
      finish(r, s, result == 2080, "returned %d, expected 2080: the word is stable here",
             result);
   } else {
      canary_armed = 0;
      finish(r, s, false, "__stack_chk_fail: %%fs:0x28 changed during the call");
   }
   akeno_elf_unload(m);
}

void
akeno_selftest_run(const akeno_mem_ops *mem, bool include_canary, akeno_selftest_result *r)
{
   memset(r, 0, sizeof(*r));
   /* The table's lookup is a binary search: sorted once, before any lookup. */
   akeno_symbol_table_seal(&selftest_table);
   snprintf(r->fixture_sha256, sizeof(r->fixture_sha256), "%s", akeno_selftest_aps2_sha256);
   r->ran = true;
   const uint64_t start = akeno_platform_now_us();
   if (mem == NULL) {
      akeno_selftest_step *s = begin(r, "code memory");
      finish(r, s, false, "the platform offers no code memory");
   } else {
      AKENO_INFO("selftest: fixture sha256 %s, page size %zu", r->fixture_sha256, mem->page_size);
      exercise_aps2(mem, r);
      exercise_relr(mem, r);
      if (include_canary)
         exercise_canary(mem, r);
   }
   r->duration_us = akeno_platform_now_us() - start;
   r->passed = r->failed_count == 0 && r->step_count > 0;
   AKENO_INFO("selftest: %s, %d steps, %d failed, %llu us", r->passed ? "PASSED" : "FAILED",
              r->step_count, r->failed_count, (unsigned long long)r->duration_us);
}
