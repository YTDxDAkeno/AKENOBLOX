/*
 * AkenoBlox PS5 - runtime status and package-check tests.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define _DEFAULT_SOURCE
#include "akeno_test.h"

#include "runtime/akeno_runtime.h"

#include <stdlib.h>
#include <unistd.h>

static void
test_stages_are_honest(void)
{
   const akeno_stage *stages;
   const size_t count = akeno_runtime_stages(&stages);
   REQUIRE(count >= 10);
   /* Nothing is claimed as verified on hardware until a console run says so
    * (docs/TEST_REPORTS.md has none yet). */
   for (size_t i = 0; i < count; i++) {
      CHECK(stages[i].evidence != AKENO_EV_HARDWARE_VERIFIED);
      CHECK(stages[i].name != NULL && stages[i].note != NULL);
   }
   const akeno_stage *blocker = akeno_runtime_first_blocker();
   REQUIRE(blocker != NULL);
   CHECK(blocker == &stages[0]);
   const akeno_stage *next = akeno_runtime_next_to_build();
   REQUIRE(next != NULL);
   CHECK(next->evidence == AKENO_EV_IN_PROGRESS || next->evidence == AKENO_EV_NOT_STARTED);
   CHECK(akeno_runtime_awaiting_hardware() >= 1);
   CHECK_EQ_STR(akeno_evidence_name(AKENO_EV_CROSS_BUILD_VERIFIED), "CROSS-BUILD VERIFIED");
}

static void
test_package_missing(void)
{
   akeno_package_check c;
   akeno_package_check_run("/nonexistent-akeno-root", &c);
   CHECK(c.searched && !c.found && !c.valid_elf);
   CHECK(strstr(c.path, "roblox/lib/x86_64/libroblox.so") != NULL);
   CHECK_EQ_STR(c.problem, "not installed");
}

static void
test_package_not_elf(void)
{
   char path[] = "/tmp/akeno-package-XXXXXX";
   const int fd = mkstemp(path);
   REQUIRE(fd >= 0);
   /* Longer than an ELF header, so it is the magic that refuses it. */
   const char junk[] = "this is not an ELF file, it is a text file, and it is long enough to be "
                       "read as far as the magic number";
   REQUIRE(write(fd, junk, sizeof(junk)) == (ssize_t)sizeof(junk));
   close(fd);
   akeno_package_check c;
   akeno_package_check_file(path, &c);
   CHECK(c.found && !c.valid_elf);
   CHECK(strstr(c.problem, "E_BAD_MAGIC") != NULL);
   unlink(path);
}

static void
test_package_fixture(void)
{
   /* A real x86-64 shared object, but not Roblox's: valid, with no Android
    * ident note, which the check says. */
   char path[512];
   snprintf(path, sizeof(path), "%s/basic_aps2_16k.so", AKENO_FIXTURE_DIR);
   akeno_package_check c;
   akeno_package_check_file(path, &c);
   CHECK(c.found && c.valid_elf && c.x86_64);
   CHECK(c.aps2 && !c.relr && !c.has_tls);
   CHECK_EQ_INT(c.import_count, 2);
   CHECK_EQ_INT(c.imports_answered, 0);
   CHECK(strstr(c.problem, "Android ident note") != NULL);

   snprintf(path, sizeof(path), "%s/needs_dep.so", AKENO_FIXTURE_DIR);
   akeno_package_check_file(path, &c);
   CHECK_EQ_INT(c.needed_count, 1);
   CHECK_EQ_STR(c.needed[0], "libakeno_fixture_dep.so");
}

static const akeno_test_case cases[] = {
   {"stages_are_honest", test_stages_are_honest},
   {"package_missing", test_package_missing},
   {"package_not_elf", test_package_not_elf},
   {"package_fixture", test_package_fixture},
};

AKENO_TEST_MAIN(cases)
