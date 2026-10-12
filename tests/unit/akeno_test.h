/*
 * AkenoBlox PS5 - a small test harness with no dependencies.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * No framework, so the same tests can later be cross-built for the console
 * and run there with nothing else linked in. Each test file defines
 * AKENO_TESTS(...) and includes this header once from its main.
 */
#ifndef AKENO_TEST_H
#define AKENO_TEST_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct akeno_test_case {
   const char *name;
   void (*run)(void);
} akeno_test_case;

static int akeno_test_failures;
static int akeno_test_checks;
static const char *akeno_test_current;

#define CHECK(condition)                                                                           \
   do {                                                                                            \
      akeno_test_checks++;                                                                         \
      if (!(condition)) {                                                                          \
         akeno_test_failures++;                                                                    \
         fprintf(stderr, "  FAIL %s (%s:%d): %s\n", akeno_test_current, __FILE__, __LINE__,         \
                 #condition);                                                                      \
      }                                                                                            \
   } while (0)

#define CHECK_EQ_INT(actual, expected)                                                             \
   do {                                                                                            \
      const long long akeno_a = (long long)(actual), akeno_e = (long long)(expected);              \
      akeno_test_checks++;                                                                         \
      if (akeno_a != akeno_e) {                                                                    \
         akeno_test_failures++;                                                                    \
         fprintf(stderr, "  FAIL %s (%s:%d): %s == %lld, expected %lld\n", akeno_test_current,     \
                 __FILE__, __LINE__, #actual, akeno_a, akeno_e);                                   \
      }                                                                                            \
   } while (0)

#define CHECK_EQ_STR(actual, expected)                                                             \
   do {                                                                                            \
      const char *akeno_a = (actual), *akeno_e = (expected);                                       \
      akeno_test_checks++;                                                                         \
      if (akeno_a == NULL || akeno_e == NULL || strcmp(akeno_a, akeno_e) != 0) {                   \
         akeno_test_failures++;                                                                    \
         fprintf(stderr, "  FAIL %s (%s:%d): %s == \"%s\", expected \"%s\"\n", akeno_test_current, \
                 __FILE__, __LINE__, #actual, akeno_a ? akeno_a : "(null)",                        \
                 akeno_e ? akeno_e : "(null)");                                                    \
      }                                                                                            \
   } while (0)

/* Stops the current test when a precondition fails, so later checks do not
 * dereference what was never set up. */
#define REQUIRE(condition)                                                                         \
   do {                                                                                            \
      akeno_test_checks++;                                                                         \
      if (!(condition)) {                                                                          \
         akeno_test_failures++;                                                                    \
         fprintf(stderr, "  FAIL %s (%s:%d): required %s\n", akeno_test_current, __FILE__,         \
                 __LINE__, #condition);                                                            \
         return;                                                                                   \
      }                                                                                            \
   } while (0)

static inline int
akeno_test_main(const akeno_test_case *cases, size_t count, int argc, char **argv)
{
   const char *only = argc > 1 ? argv[1] : NULL;
   int ran = 0;
   for (size_t i = 0; i < count; i++) {
      if (only != NULL && strstr(cases[i].name, only) == NULL)
         continue;
      akeno_test_current = cases[i].name;
      const int before = akeno_test_failures;
      cases[i].run();
      ran++;
      printf("%s %s\n", akeno_test_failures == before ? "  ok  " : "  FAIL", cases[i].name);
   }
   printf("%d tests, %d checks, %d failures\n", ran, akeno_test_checks, akeno_test_failures);
   return akeno_test_failures == 0 && ran > 0 ? 0 : 1;
}

#define AKENO_TEST_MAIN(cases)                                                                     \
   int main(int argc, char **argv)                                                                 \
   {                                                                                               \
      return akeno_test_main(cases, sizeof(cases) / sizeof(cases[0]), argc, argv);                 \
   }

/* Reads a whole file into memory; NULL on failure. */
static inline uint8_t *
akeno_test_read_file(const char *path, size_t *size)
{
   FILE *f = fopen(path, "rb");
   if (f == NULL)
      return NULL;
   if (fseek(f, 0, SEEK_END) != 0) {
      fclose(f);
      return NULL;
   }
   const long length = ftell(f);
   if (length < 0 || fseek(f, 0, SEEK_SET) != 0) {
      fclose(f);
      return NULL;
   }
   uint8_t *bytes = malloc(length > 0 ? (size_t)length : 1);
   if (bytes != NULL && fread(bytes, 1, (size_t)length, f) != (size_t)length) {
      free(bytes);
      bytes = NULL;
   }
   fclose(f);
   *size = (size_t)length;
   return bytes;
}

#endif /* AKENO_TEST_H */
