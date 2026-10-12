/*
 * AkenoBlox PS5 - akeno_elf tests: loading real lld-linked objects, and
 * refusing broken ones.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Every fixture is a real shared object built by tests/CMakeLists.txt with
 * clang and lld for x86_64-linux-android26, so APS2 and RELR are what lld
 * emits, not hand-made bytes. The malformed cases start from those files and
 * damage one field each, then a deterministic mutation sweep damages bytes at
 * random; under AddressSanitizer every refusal must be clean.
 */
#define _DEFAULT_SOURCE
#include "akeno_test.h"

#include "akeno_elf.h"
#include "akeno_host.h"
#include "elf_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef AKENO_FIXTURE_DIR
#error "AKENO_FIXTURE_DIR must name the directory the fixtures were built into"
#endif

/* ------------------------------------------------------------ the host */

static char events[256];

static int
host_add(int a, int b)
{
   return a + b + 1000;
}

static void
host_note(const char *event)
{
   const size_t used = strlen(events);
   snprintf(events + used, sizeof(events) - used, "%s;", event);
}

static int stack_chk_failures;

static void
host_stack_chk_fail(void)
{
   stack_chk_failures++;
   fprintf(stderr, "__stack_chk_fail called from a fixture\n");
   abort();
}

static akeno_symbol host_symbols[] = {
   {"akeno_host_note", (void *)host_note},
   {"akeno_host_add", (void *)host_add},
   {"__stack_chk_fail", (void *)host_stack_chk_fail},
};
static akeno_symbol_table host_table = {"libakeno_host.so", host_symbols,
                                        sizeof(host_symbols) / sizeof(host_symbols[0])};

static void *
resolve_host(void *ctx, const char *name, const char *version)
{
   (void)ctx;
   (void)version;
   return akeno_symbol_table_find(&host_table, name);
}

static uint8_t *
fixture(const char *name, size_t *size)
{
   char path[512];
   snprintf(path, sizeof(path), "%s/%s", AKENO_FIXTURE_DIR, name);
   uint8_t *bytes = akeno_test_read_file(path, size);
   if (bytes == NULL)
      fprintf(stderr, "  cannot read fixture %s\n", path);
   return bytes;
}

static akeno_elf_load_options
default_options(void)
{
   /* Lookups are binary searches; sealing sorts the table (idempotent). */
   akeno_symbol_table_seal(&host_table);
   return (akeno_elf_load_options){
      .mem = akeno_host_code_memory(),
      .resolve = resolve_host,
      .run_constructors = true,
   };
}

/* The permissions the kernel reports for address, from /proc/self/maps. */
static bool
kernel_permissions(const void *address, char permissions[5])
{
   FILE *maps = fopen("/proc/self/maps", "r");
   if (maps == NULL)
      return false;
   char line[512];
   bool found = false;
   while (fgets(line, sizeof(line), maps) != NULL) {
      unsigned long long start, end;
      char perms[5];
      if (sscanf(line, "%llx-%llx %4s", &start, &end, perms) != 3)
         continue;
      if ((uintptr_t)address >= start && (uintptr_t)address < end) {
         memcpy(permissions, perms, 5);
         found = true;
         break;
      }
   }
   fclose(maps);
   return found;
}

/* --------------------------------------------------------- loading */

typedef int (*int_void)(void);
typedef int (*int_int_int)(int, int);
typedef const char *(*str_void)(void);
typedef int *(*intp_void)(void);
typedef void *(*ptr_void)(void);
typedef void (*void_int_int)(int, int);

static void
exercise_basic(const char *name, const akeno_mem_ops *mem)
{
   size_t size;
   uint8_t *image = fixture(name, &size);
   REQUIRE(image != NULL);
   events[0] = '\0';
   akeno_elf_load_options opt = default_options();
   if (mem != NULL)
      opt.mem = mem;
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   const akeno_elf_status st = akeno_elf_load(image, size, &opt, &m, &report);
   free(image); /* the module must not need the image after loading */
   if (st != AKENO_ELF_OK)
      fprintf(stderr, "  %s: %s: %s\n", name, akeno_elf_status_name(st), report.message);
   REQUIRE(st == AKENO_ELF_OK);
   CHECK(report.relative_applied >= 4);
   CHECK(report.symbolic_applied >= 2);
   CHECK_EQ_STR(events, "init;");

   int_void counter = (int_void)akeno_elf_symbol(m, "fixture_counter");
   int_int_int add = (int_int_int)akeno_elf_symbol(m, "fixture_add");
   int_int_int apply = (int_int_int)akeno_elf_symbol(m, "fixture_apply");
   str_void greet = (str_void)akeno_elf_symbol(m, "fixture_greet");
   int_void sum = (int_void)akeno_elf_symbol(m, "fixture_sum_data");
   void_int_int write = (void_int_int)akeno_elf_symbol(m, "fixture_write_data");
   ptr_void import_address = (ptr_void)akeno_elf_symbol(m, "fixture_import_address");
   int *data = akeno_elf_symbol(m, "fixture_data");
   int_void *counter_pointer = akeno_elf_symbol(m, "fixture_counter_pointer");
   REQUIRE(counter && add && apply && greet && sum && write && import_address && data &&
           counter_pointer);
   CHECK(akeno_elf_symbol(m, "fixture_init") == NULL); /* static: not exported */
   CHECK(akeno_elf_symbol(m, "no_such_symbol") == NULL);

   CHECK_EQ_INT(counter(), 42);           /* the constructor ran */
   CHECK_EQ_INT(add(2, 3), 1005);         /* through the PLT into the host */
   CHECK_EQ_INT(apply(0, 5), 10);         /* relative pointers in RELRO */
   CHECK_EQ_INT(apply(1, 5), 15);
   CHECK_EQ_STR(greet(), "akeno");        /* a relative pointer in .data */
   CHECK_EQ_INT(sum(), 10);               /* read data */
   write(2, 100);                         /* write data */
   CHECK_EQ_INT(data[2], 100);
   CHECK_EQ_INT(sum(), 107);
   CHECK(import_address() == (void *)host_add); /* GLOB_DAT */
   CHECK(*counter_pointer == counter);          /* symbolic R_X86_64_64 */

   /* Permissions: the kernel's view and the loader's agree with the segments. */
   char perms[5];
   CHECK(akeno_elf_page_protection(m, (const void *)counter) ==
         (AKENO_PROT_READ | AKENO_PROT_EXEC));
   CHECK(kernel_permissions((const void *)counter, perms) && perms[0] == 'r' && perms[1] == '-' &&
         perms[2] == 'x');
   CHECK(akeno_elf_page_protection(m, data) & AKENO_PROT_WRITE);
   CHECK(kernel_permissions(data, perms) && perms[1] == 'w' && perms[2] == '-');
   CHECK(akeno_elf_page_protection(m, (const uint8_t *)akeno_elf_base(m) + akeno_elf_span(m)) ==
         AKENO_PROT_NONE);

   akeno_elf_unload(m);
   CHECK_EQ_STR(events, "init;fini;"); /* the destructor ran, once */
}

static void test_load_rela_4k(void) { exercise_basic("basic_rela_4k.so", NULL); }
static void test_load_rela_16k(void) { exercise_basic("basic_rela_16k.so", NULL); }
static void test_load_aps2_4k(void) { exercise_basic("basic_aps2_4k.so", NULL); }
static void test_load_aps2_16k(void) { exercise_basic("basic_aps2_16k.so", NULL); }
static void test_load_relr(void) { exercise_basic("basic_relr_4k.so", NULL); }
static void test_load_android_relr(void) { exercise_basic("basic_android_relr.so", NULL); }
static void test_load_sysv_hash(void) { exercise_basic("basic_sysv.so", NULL); }
static void test_load_both_hashes(void) { exercise_basic("basic_both_hash.so", NULL); }

/* The console's pages are 16 KiB: a library aligned for 16 KiB keeps W^X. */
static void
test_load_console_pages(void)
{
   exercise_basic("basic_aps2_16k.so", akeno_host_code_memory_paged(16384));
   size_t size;
   uint8_t *image = fixture("basic_aps2_16k.so", &size);
   REQUIRE(image != NULL);
   akeno_elf_load_options opt = default_options();
   opt.mem = akeno_host_code_memory_paged(16384);
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   REQUIRE(akeno_elf_load(image, size, &opt, &m, &report) == AKENO_ELF_OK);
   CHECK_EQ_INT(report.wx_pages, 0);
   CHECK((akeno_elf_base(m) & 16383u) == 0);
   akeno_elf_unload(m);
   free(image);
}

/* A library aligned for 4 KiB on 16 KiB pages: segments share pages, which
 * get the union of their permissions; it still runs, and says so. */
static void
test_load_4k_library_on_console_pages(void)
{
   size_t size;
   uint8_t *image = fixture("basic_rela_4k.so", &size);
   REQUIRE(image != NULL);
   akeno_elf_load_options opt = default_options();
   opt.mem = akeno_host_code_memory_paged(16384);
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   events[0] = '\0';
   REQUIRE(akeno_elf_load(image, size, &opt, &m, &report) == AKENO_ELF_OK);
   CHECK(report.wx_pages > 0);
   int_int_int add = (int_int_int)akeno_elf_symbol(m, "fixture_add");
   REQUIRE(add != NULL);
   CHECK_EQ_INT(add(20, 22), 1042);
   akeno_elf_unload(m);
   free(image);
}

static void
test_parse_summary(void)
{
   size_t size;
   uint8_t *image = fixture("basic_aps2_16k.so", &size);
   REQUIRE(image != NULL);
   akeno_elf_info info;
   char err[160];
   REQUIRE(akeno_elf_parse(image, size, &info, err, sizeof(err)) == AKENO_ELF_OK);
   CHECK_EQ_INT(info.machine, 62);
   CHECK(info.has_dynamic && info.has_gnu_hash && info.has_relro && !info.has_tls);
   CHECK(info.relocs.android_rela_bytes > 0);
   CHECK_EQ_INT(info.relocs.rela_bytes, 0);
   CHECK_EQ_INT(info.init_array_count, 1);
   CHECK_EQ_INT(info.fini_array_count, 1);
   CHECK(info.max_align >= 16384);
   CHECK(info.load_count >= 2);
   CHECK(info.has_soname);
   CHECK_EQ_STR(akeno_elf_dynstr(&info, image, size, info.soname), "libakeno_fixture_basic.so");
   free(image);

   image = fixture("basic_relr_4k.so", &size);
   REQUIRE(image != NULL);
   REQUIRE(akeno_elf_parse(image, size, &info, err, sizeof(err)) == AKENO_ELF_OK);
   CHECK(info.relocs.relr_bytes > 0);
   free(image);
}

struct import_list {
   char names[8][64];
   int count;
};

static bool
collect_import(const akeno_elf_import *import, void *ctx)
{
   struct import_list *list = ctx;
   if (list->count < 8)
      snprintf(list->names[list->count++], 64, "%s", import->name);
   return true;
}

static bool
list_has(const struct import_list *list, const char *name)
{
   for (int i = 0; i < list->count; i++)
      if (strcmp(list->names[i], name) == 0)
         return true;
   return false;
}

static void
test_imports_listed_before_loading(void)
{
   size_t size;
   uint8_t *image = fixture("basic_rela_4k.so", &size);
   REQUIRE(image != NULL);
   akeno_elf_info info;
   REQUIRE(akeno_elf_parse(image, size, &info, NULL, 0) == AKENO_ELF_OK);
   struct import_list list = {0};
   CHECK_EQ_INT(akeno_elf_for_each_import(&info, image, size, collect_import, &list), 2);
   CHECK(list_has(&list, "akeno_host_add"));
   CHECK(list_has(&list, "akeno_host_note"));
   free(image);
}

static void
test_dependency_resolution(void)
{
   size_t dep_size, main_size;
   uint8_t *dep_image = fixture("dep.so", &dep_size);
   uint8_t *main_image = fixture("needs_dep.so", &main_size);
   REQUIRE(dep_image != NULL && main_image != NULL);

   akeno_elf_info info;
   REQUIRE(akeno_elf_parse(main_image, main_size, &info, NULL, 0) == AKENO_ELF_OK);
   REQUIRE(info.needed_count == 1);
   CHECK_EQ_STR(akeno_elf_dynstr(&info, main_image, main_size, info.needed[0]),
                "libakeno_fixture_dep.so");

   /* Without the dependency the load names what is missing. */
   akeno_elf_load_options opt = default_options();
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   CHECK(akeno_elf_load(main_image, main_size, &opt, &m, &report) == AKENO_ELF_E_UNDEFINED_SYMBOL);
   CHECK(m == NULL);
   CHECK_EQ_STR(report.missing[0], "dep_value");

   /* With it: dependencies first, then the library, through a chain. */
   akeno_elf_module *dep = NULL;
   REQUIRE(akeno_elf_load(dep_image, dep_size, &opt, &dep, NULL) == AKENO_ELF_OK);
   akeno_resolver_chain chain = {.modules = {dep}, .module_count = 1,
                                 .tables = {&host_table}, .table_count = 1};
   opt.resolve = akeno_resolver_chain_resolve;
   opt.resolve_ctx = &chain;
   REQUIRE(akeno_elf_load(main_image, main_size, &opt, &m, &report) == AKENO_ELF_OK);
   int_void value = (int_void)akeno_elf_symbol(m, "needs_dep_value");
   REQUIRE(value != NULL);
   CHECK_EQ_INT(value(), 42);
   akeno_elf_unload(m);
   akeno_elf_unload(dep);
   free(dep_image);
   free(main_image);
}

static void
test_ifunc(void)
{
   size_t size;
   uint8_t *image = fixture("ifunc.so", &size);
   REQUIRE(image != NULL);
   akeno_elf_load_options opt = default_options();
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   /* Refused by default: a resolver is code running inside the load. */
   CHECK(akeno_elf_load(image, size, &opt, &m, &report) == AKENO_ELF_E_IFUNC_REFUSED);
   CHECK(m == NULL);
   opt.allow_ifunc = true;
   REQUIRE(akeno_elf_load(image, size, &opt, &m, &report) == AKENO_ELF_OK);
   CHECK(report.deferred_applied >= 1);
   int_void call = (int_void)akeno_elf_symbol(m, "call_picked");
   int_void call_hidden = (int_void)akeno_elf_symbol(m, "call_hidden_picked");
   REQUIRE(call != NULL && call_hidden != NULL);
   CHECK_EQ_INT(call(), 11);
   CHECK_EQ_INT(call_hidden(), 12);
   akeno_elf_unload(m);
   free(image);
}

static void
test_tls(void)
{
   size_t size;
   uint8_t *image = fixture("tls_gnu.so", &size);
   REQUIRE(image != NULL);
   akeno_elf_load_options opt = default_options();
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   CHECK(akeno_elf_load(image, size, &opt, &m, &report) == AKENO_ELF_E_TLS_UNSUPPORTED);
   free(image);

   /* For Android API 26 the compiler emulates TLS: no PT_TLS, an import of
    * __emutls_get_address instead, which a runtime answers (or names). */
   image = fixture("tls_android.so", &size);
   REQUIRE(image != NULL);
   akeno_elf_info info;
   REQUIRE(akeno_elf_parse(image, size, &info, NULL, 0) == AKENO_ELF_OK);
   CHECK(!info.has_tls);
   CHECK(akeno_elf_load(image, size, &opt, &m, &report) == AKENO_ELF_E_UNDEFINED_SYMBOL);
   CHECK_EQ_STR(report.missing[0], "__emutls_get_address");
   free(image);
}

static void
test_missing_symbols_all_named(void)
{
   size_t size;
   uint8_t *image = fixture("missing.so", &size);
   REQUIRE(image != NULL);
   akeno_elf_load_options opt = default_options();
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   CHECK(akeno_elf_load(image, size, &opt, &m, &report) == AKENO_ELF_E_UNDEFINED_SYMBOL);
   CHECK(m == NULL);
   CHECK_EQ_INT(report.missing_count, 2); /* the weak import is not missing */
   bool not_provided = false, also_missing = false;
   for (uint32_t i = 0; i < report.missing_count && i < AKENO_ELF_MISSING_KEPT; i++) {
      not_provided |= strcmp(report.missing[i], "akeno_not_provided") == 0;
      also_missing |= strcmp(report.missing[i], "akeno_also_missing") == 0;
   }
   CHECK(not_provided && also_missing);
   CHECK(strstr(report.message, "akeno_not_provided") != NULL);
   /* Without a report the load still fails. */
   CHECK(akeno_elf_load(image, size, &opt, &m, NULL) == AKENO_ELF_E_UNDEFINED_SYMBOL);
   free(image);
}

static void
test_weak_import_binds_null(void)
{
   size_t size;
   uint8_t *image = fixture("weak.so", &size);
   REQUIRE(image != NULL);
   akeno_elf_load_options opt = default_options();
   akeno_elf_module *m = NULL;
   REQUIRE(akeno_elf_load(image, size, &opt, &m, NULL) == AKENO_ELF_OK);
   int_void is_null = (int_void)akeno_elf_symbol(m, "weak_is_null");
   int (*call_or)(int) = (int (*)(int))akeno_elf_symbol(m, "weak_call_or");
   REQUIRE(is_null != NULL && call_or != NULL);
   CHECK_EQ_INT(is_null(), 1);
   CHECK_EQ_INT(call_or(9), 9);
   akeno_elf_unload(m);
   free(image);
}

/* Android's stack protector reads %fs:0x28; on a glibc host that word is
 * glibc's own guard, so it is stable and the protected function returns. */
static void
test_android_stack_protector_on_host(void)
{
   size_t size;
   uint8_t *image = fixture("canary.so", &size);
   REQUIRE(image != NULL);
   akeno_elf_load_options opt = default_options();
   akeno_elf_module *m = NULL;
   events[0] = '\0';
   REQUIRE(akeno_elf_load(image, size, &opt, &m, NULL) == AKENO_ELF_OK);
   int (*sum)(int) = (int (*)(int))akeno_elf_symbol(m, "canary_protected_sum");
   REQUIRE(sum != NULL);
   CHECK_EQ_INT(sum(1), 2080); /* sum of 1..64 */
   CHECK_EQ_INT(stack_chk_failures, 0);
   akeno_elf_unload(m);
   free(image);
}

/* ------------------------------------------------- memory: failures */

struct tracking {
   akeno_mem_ops ops;
   const akeno_mem_ops *inner;
   int reserves, releases, protects;
   bool fail_reserve;
   int fail_protect_at; /* 1-based protect call to refuse; 0 never */
};

static int
tracking_reserve(void *ctx, size_t size, size_t align, unsigned flags, akeno_mem_region *out)
{
   struct tracking *t = ctx;
   if (t->fail_reserve)
      return 12; /* ENOMEM */
   const int rc = t->inner->reserve(t->inner->ctx, size, align, flags, out);
   if (rc == 0)
      t->reserves++;
   return rc;
}

static int
tracking_protect(void *ctx, const akeno_mem_region *r, size_t offset, size_t length, unsigned prot,
                 unsigned *applied)
{
   struct tracking *t = ctx;
   t->protects++;
   if (t->fail_protect_at != 0 && t->protects == t->fail_protect_at)
      return 13; /* EACCES */
   return t->inner->protect(t->inner->ctx, r, offset, length, prot, applied);
}

static void
tracking_release(void *ctx, akeno_mem_region *r)
{
   struct tracking *t = ctx;
   t->releases++;
   t->inner->release(t->inner->ctx, r);
}

static struct tracking
tracking_ops(void)
{
   struct tracking t = {.inner = akeno_host_code_memory()};
   t.ops = (akeno_mem_ops){t.inner->page_size, tracking_reserve, tracking_protect,
                           tracking_release, NULL};
   return t;
}

static void
test_out_of_memory(void)
{
   size_t size;
   uint8_t *image = fixture("basic_rela_4k.so", &size);
   REQUIRE(image != NULL);
   struct tracking t = tracking_ops();
   t.ops.ctx = &t;
   t.fail_reserve = true;
   akeno_elf_load_options opt = default_options();
   opt.mem = &t.ops;
   akeno_elf_module *m = NULL;
   akeno_elf_load_report report;
   CHECK(akeno_elf_load(image, size, &opt, &m, &report) == AKENO_ELF_E_NO_MEMORY);
   CHECK(m == NULL);
   CHECK_EQ_INT(t.reserves, 0);

   /* A refused protection releases what was reserved. */
   t.fail_reserve = false;
   t.fail_protect_at = 1;
   CHECK(akeno_elf_load(image, size, &opt, &m, &report) == AKENO_ELF_E_PROTECT_FAILED);
   CHECK_EQ_INT(t.reserves, 1);
   CHECK_EQ_INT(t.releases, 1);

   /* A span past the caller's limit is refused before anything is reserved. */
   t.fail_protect_at = 0;
   opt.max_span = 4096;
   CHECK(akeno_elf_load(image, size, &opt, &m, &report) == AKENO_ELF_E_TOO_LARGE);
   CHECK_EQ_INT(t.reserves, 1);
   free(image);
}

static void
test_cleanup_balances(void)
{
   size_t size;
   uint8_t *image = fixture("basic_aps2_4k.so", &size);
   REQUIRE(image != NULL);
   struct tracking t = tracking_ops();
   t.ops.ctx = &t;
   akeno_elf_load_options opt = default_options();
   opt.mem = &t.ops;
   for (int round = 0; round < 50; round++) {
      akeno_elf_module *m = NULL;
      events[0] = '\0';
      REQUIRE(akeno_elf_load(image, size, &opt, &m, NULL) == AKENO_ELF_OK);
      akeno_elf_unload(m);
   }
   CHECK_EQ_INT(t.reserves, 50);
   CHECK_EQ_INT(t.releases, 50);
   akeno_elf_unload(NULL); /* ignored */
   free(image);
}

/* ---------------------------------------------------- malformed input */

static akeno_elf_status
load_bytes(uint8_t *image, size_t size)
{
   akeno_elf_load_options opt = default_options();
   opt.run_constructors = false;
   akeno_elf_module *m = NULL;
   const akeno_elf_status st = akeno_elf_load(image, size, &opt, &m, NULL);
   akeno_elf_unload(m);
   return st;
}

static elf64_dyn *
find_dynamic(uint8_t *image, size_t size, int64_t tag)
{
   akeno_elf_info info;
   if (akeno_elf_parse(image, size, &info, NULL, 0) != AKENO_ELF_OK)
      return NULL;
   uint64_t offset;
   if (!elf_vaddr_to_offset(&info, info.dyn_vaddr, info.dyn_count * sizeof(elf64_dyn), &offset))
      return NULL;
   for (uint64_t i = 0; i < info.dyn_count; i++) {
      elf64_dyn *d = (elf64_dyn *)(image + offset + i * sizeof(elf64_dyn));
      if (d->d_tag == tag)
         return d;
      if (d->d_tag == 0)
         break;
   }
   return NULL;
}

static elf64_phdr *
find_phdr(uint8_t *image, uint32_t type, int nth)
{
   elf64_ehdr *eh = (elf64_ehdr *)image;
   for (int i = 0; i < eh->e_phnum; i++) {
      elf64_phdr *ph = (elf64_phdr *)(image + eh->e_phoff + (size_t)i * sizeof(elf64_phdr));
      if (ph->p_type == type && nth-- == 0)
         return ph;
   }
   return NULL;
}

static void
test_invalid_headers(void)
{
   size_t size;
   uint8_t *original = fixture("basic_rela_4k.so", &size);
   REQUIRE(original != NULL);
   uint8_t *image = malloc(size);
   REQUIRE(image != NULL);
   akeno_elf_info info;

   CHECK(akeno_elf_parse(NULL, size, &info, NULL, 0) == AKENO_ELF_E_INVALID_ARGUMENT);
   CHECK(akeno_elf_parse(original, 10, &info, NULL, 0) == AKENO_ELF_E_TRUNCATED);
   CHECK(akeno_elf_load(original, size, NULL, NULL, NULL) == AKENO_ELF_E_INVALID_ARGUMENT);

   struct {
      size_t offset;
      uint8_t value;
      akeno_elf_status expected;
   } byte_cases[] = {
      {0, 0x7e, AKENO_ELF_E_BAD_MAGIC},
      {4, 1, AKENO_ELF_E_BAD_CLASS},
      {5, 2, AKENO_ELF_E_BAD_ENCODING},
      {6, 0, AKENO_ELF_E_BAD_VERSION},
      {16, 2, AKENO_ELF_E_BAD_TYPE},         /* ET_EXEC */
      {18, 183, AKENO_ELF_E_BAD_MACHINE},    /* EM_AARCH64: the arm64-v8a build */
      {54, 32, AKENO_ELF_E_BAD_PHDR},        /* e_phentsize */
   };
   for (size_t i = 0; i < sizeof(byte_cases) / sizeof(byte_cases[0]); i++) {
      memcpy(image, original, size);
      image[byte_cases[i].offset] = byte_cases[i].value;
      CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), byte_cases[i].expected);
      CHECK_EQ_INT(load_bytes(image, size), byte_cases[i].expected);
   }

   memcpy(image, original, size);
   ((elf64_ehdr *)image)->e_phnum = 0;
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_BAD_PHDR);
   memcpy(image, original, size);
   ((elf64_ehdr *)image)->e_phoff = size - 8;
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_TRUNCATED);
   free(image);
   free(original);
}

static void
test_invalid_segments(void)
{
   size_t size;
   uint8_t *original = fixture("basic_rela_4k.so", &size);
   REQUIRE(original != NULL);
   uint8_t *image = malloc(size);
   REQUIRE(image != NULL);
   akeno_elf_info info;
   char err[160];

   memcpy(image, original, size);
   elf64_phdr *load = find_phdr(image, PT_LOAD, 1);
   REQUIRE(load != NULL);
   load->p_filesz = load->p_memsz + 1;
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, err, sizeof(err)), AKENO_ELF_E_BAD_SEGMENT);

   memcpy(image, original, size);
   load = find_phdr(image, PT_LOAD, 1);
   load->p_offset = size; /* file bytes past the end */
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_TRUNCATED);

   memcpy(image, original, size);
   load = find_phdr(image, PT_LOAD, 1);
   load->p_align = 3000;
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_BAD_SEGMENT);

   memcpy(image, original, size);
   load = find_phdr(image, PT_LOAD, 1);
   load->p_vaddr += 8; /* no longer congruent with p_offset */
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_BAD_SEGMENT);

   memcpy(image, original, size);
   elf64_phdr *first = find_phdr(image, PT_LOAD, 0), *second = find_phdr(image, PT_LOAD, 1);
   second->p_vaddr = first->p_vaddr; /* overlap */
   second->p_offset = first->p_offset;
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_BAD_SEGMENT);

   memcpy(image, original, size);
   for (int i = 0; i < 8; i++) {
      elf64_phdr *ph = find_phdr(image, PT_LOAD, 0);
      if (ph == NULL)
         break;
      ph->p_type = PT_NULL;
   }
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_NO_LOAD);

   memcpy(image, original, size);
   /* PT_GNU_STACK (every fixture has one, from -z noexecstack) relabelled. */
   elf64_phdr *tls_like = find_phdr(image, PT_GNU_STACK, 0);
   if (tls_like != NULL) {
      tls_like->p_type = PT_TLS;
      CHECK_EQ_INT(load_bytes(image, size), AKENO_ELF_E_TLS_UNSUPPORTED);
   }
   CHECK(tls_like != NULL);
   free(image);
   free(original);
}

static void
test_invalid_dynamic(void)
{
   size_t size;
   uint8_t *original = fixture("basic_rela_4k.so", &size);
   REQUIRE(original != NULL);
   uint8_t *image = malloc(size);
   REQUIRE(image != NULL);
   akeno_elf_info info;
   elf64_dyn *d;

   memcpy(image, original, size);
   REQUIRE((d = find_dynamic(image, size, DT_STRSZ)) != NULL);
   d->d_val = UINT64_C(1) << 40;
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_BAD_STRTAB);

   memcpy(image, original, size);
   REQUIRE((d = find_dynamic(image, size, DT_SYMENT)) != NULL);
   d->d_val = 16;
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_BAD_SYMTAB);

   memcpy(image, original, size);
   REQUIRE((d = find_dynamic(image, size, DT_GNU_HASH)) != NULL);
   REQUIRE(akeno_elf_parse(original, size, &info, NULL, 0) == AKENO_ELF_OK);
   uint64_t hash_offset;
   REQUIRE(elf_vaddr_to_offset(&info, info.gnu_hash_vaddr, 16, &hash_offset));
   memset(image + hash_offset, 0, 4); /* nbuckets = 0 */
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_BAD_HASH);

   memcpy(image, original, size);
   REQUIRE((d = find_dynamic(image, size, DT_INIT_ARRAYSZ)) != NULL);
   d->d_val = 12;
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_BAD_DYNAMIC);

   memcpy(image, original, size);
   REQUIRE((d = find_dynamic(image, size, DT_RELASZ)) != NULL);
   d->d_val = size * 4;
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_BAD_RELOC);

   /* No DT_NULL anywhere in the section. */
   memcpy(image, original, size);
   REQUIRE(akeno_elf_parse(image, size, &info, NULL, 0) == AKENO_ELF_OK);
   uint64_t dyn_offset;
   REQUIRE(elf_vaddr_to_offset(&info, info.dyn_vaddr, info.dyn_count * 16, &dyn_offset));
   for (uint64_t i = 0; i < info.dyn_count; i++) {
      elf64_dyn *entry = (elf64_dyn *)(image + dyn_offset + i * 16);
      if (entry->d_tag == DT_NULL)
         entry->d_tag = 0x7ffffff0; /* an unknown tag, skipped */
   }
   CHECK_EQ_INT(akeno_elf_parse(image, size, &info, NULL, 0), AKENO_ELF_E_BAD_DYNAMIC);
   free(image);
   free(original);
}

static void
test_malformed_relocations(void)
{
   size_t size;
   uint8_t *original = fixture("basic_rela_4k.so", &size);
   REQUIRE(original != NULL);
   uint8_t *image = malloc(size);
   REQUIRE(image != NULL);
   akeno_elf_info info;
   REQUIRE(akeno_elf_parse(original, size, &info, NULL, 0) == AKENO_ELF_OK);
   REQUIRE(info.relocs.rela_bytes >= sizeof(elf64_rela));
   uint64_t rela_offset;
   REQUIRE(elf_vaddr_to_offset(&info, info.rela_vaddr, info.relocs.rela_bytes, &rela_offset));

   memcpy(image, original, size);
   elf64_rela *r = (elf64_rela *)(image + rela_offset);
   r->r_info = (r->r_info & ~UINT64_C(0xffffffff)) | 99u; /* no such type */
   CHECK_EQ_INT(load_bytes(image, size), AKENO_ELF_E_UNSUPPORTED_RELOC);

   memcpy(image, original, size);
   r = (elf64_rela *)(image + rela_offset);
   r->r_offset = UINT64_C(1) << 40; /* outside the image */
   CHECK_EQ_INT(load_bytes(image, size), AKENO_ELF_E_BAD_RELOC);

   memcpy(image, original, size);
   r = (elf64_rela *)(image + rela_offset);
   r->r_info = ((uint64_t)0xffffff << 32) | R_X86_64_GLOB_DAT; /* symbol out of range */
   CHECK_EQ_INT(load_bytes(image, size), AKENO_ELF_E_BAD_RELOC);

   memcpy(image, original, size);
   r = (elf64_rela *)(image + rela_offset);
   r->r_info = (r->r_info & ~UINT64_C(0xffffffff)) | R_X86_64_COPY;
   CHECK_EQ_INT(load_bytes(image, size), AKENO_ELF_E_UNSUPPORTED_RELOC);

   memcpy(image, original, size);
   r = (elf64_rela *)(image + rela_offset);
   r->r_info = (r->r_info & ~UINT64_C(0xffffffff)) | R_X86_64_TPOFF64;
   CHECK_EQ_INT(load_bytes(image, size), AKENO_ELF_E_TLS_UNSUPPORTED);
   free(image);
   free(original);

   /* APS2: a damaged magic, and a stream cut short. */
   original = fixture("basic_aps2_4k.so", &size);
   REQUIRE(original != NULL);
   image = malloc(size);
   REQUIRE(image != NULL);
   REQUIRE(akeno_elf_parse(original, size, &info, NULL, 0) == AKENO_ELF_OK);
   uint64_t aps2_offset;
   REQUIRE(elf_vaddr_to_offset(&info, info.android_rela_vaddr, 4, &aps2_offset));
   memcpy(image, original, size);
   image[aps2_offset + 3] = '3';
   CHECK_EQ_INT(load_bytes(image, size), AKENO_ELF_E_BAD_RELOC);
   memcpy(image, original, size);
   elf64_dyn *d = find_dynamic(image, size, DT_ANDROID_RELASZ);
   REQUIRE(d != NULL);
   d->d_val = 6;
   CHECK_EQ_INT(load_bytes(image, size), AKENO_ELF_E_BAD_RELOC);
   free(image);
   free(original);

   /* RELR: a bitmap before any address. */
   original = fixture("basic_relr_4k.so", &size);
   REQUIRE(original != NULL);
   image = malloc(size);
   REQUIRE(image != NULL);
   REQUIRE(akeno_elf_parse(original, size, &info, NULL, 0) == AKENO_ELF_OK);
   uint64_t relr_offset;
   REQUIRE(elf_vaddr_to_offset(&info, info.relr_vaddr, 8, &relr_offset));
   memcpy(image, original, size);
   image[relr_offset] |= 1;
   CHECK_EQ_INT(load_bytes(image, size), AKENO_ELF_E_BAD_RELOC);
   free(image);
   free(original);
}

/* Every prefix of a valid file is refused or loaded, never a fault. */
static void
test_truncation_sweep(void)
{
   size_t size;
   uint8_t *original = fixture("basic_aps2_4k.so", &size);
   REQUIRE(original != NULL);
   int refused = 0;
   for (size_t length = 0; length < size; length += (length < 256 ? 1 : 7)) {
      uint8_t *image = malloc(length ? length : 1);
      REQUIRE(image != NULL);
      memcpy(image, original, length);
      if (load_bytes(image, length) != AKENO_ELF_OK)
         refused++;
      free(image);
   }
   CHECK(refused > 0);
   free(original);
}

/* Deterministic random damage: under ASan this is a small fuzz run in CI. */
static void
test_mutation_sweep(void)
{
   const char *names[] = {"basic_rela_4k.so", "basic_aps2_16k.so", "basic_relr_4k.so",
                          "ifunc.so"};
   uint64_t state = 0x9e3779b97f4a7c15u;
   int outcomes[AKENO_ELF_STATUS_COUNT] = {0};
   for (size_t n = 0; n < sizeof(names) / sizeof(names[0]); n++) {
      size_t size;
      uint8_t *original = fixture(names[n], &size);
      REQUIRE(original != NULL);
      uint8_t *image = malloc(size);
      REQUIRE(image != NULL);
      for (int round = 0; round < 600; round++) {
         memcpy(image, original, size);
         const int flips = 1 + round % 4;
         for (int f = 0; f < flips; f++) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            /* Bias towards the headers and tables, where damage matters. */
            const size_t limit = (state >> 40) % 4 == 0 ? size : (size < 2048 ? size : 2048);
            image[state % limit] ^= (uint8_t)(1u << ((state >> 32) % 8));
         }
         const akeno_elf_status st = load_bytes(image, size);
         outcomes[st < AKENO_ELF_STATUS_COUNT ? st : 0]++;
      }
      free(image);
      free(original);
   }
   int distinct = 0;
   for (int i = 0; i < AKENO_ELF_STATUS_COUNT; i++)
      distinct += outcomes[i] > 0;
   CHECK(distinct >= 4); /* the sweep reached several different refusals */
}

/* Inputs the fuzzers once crashed on, kept so they never do again. Each is
 * loaded without constructors, then looked up in, as fuzz_elf_load does. */
static void
test_fuzzer_regressions(void)
{
   static const char *const inputs[] = {
      /* a hash table pointed at a gap page left without access */
      "symbol-table-in-unreadable-page.bin",
   };
   for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
      char path[512];
      snprintf(path, sizeof(path), "%s/%s", AKENO_REGRESS_DIR, inputs[i]);
      size_t size;
      uint8_t *image = akeno_test_read_file(path, &size);
      REQUIRE(image != NULL);
      akeno_elf_load_options opt = default_options();
      opt.run_constructors = false;
      akeno_elf_module *m = NULL;
      if (akeno_elf_load(image, size, &opt, &m, NULL) == AKENO_ELF_OK) {
         (void)akeno_elf_symbol(m, "fixture_add");
         (void)akeno_elf_symbol(m, "");
      }
      akeno_elf_unload(m);
      free(image);
      CHECK(true); /* reaching here is the test */
   }
}

/* ------------------------------------------------------------ tables */

static void
test_symbol_tables(void)
{
   akeno_symbol symbols[] = {{"zeta", (void *)1}, {"alpha", (void *)2}, {"mu", (void *)3}};
   akeno_symbol_table table = {"libtest.so", symbols, 3};
   REQUIRE(akeno_symbol_table_seal(&table));
   CHECK(akeno_symbol_table_find(&table, "alpha") == (void *)2);
   CHECK(akeno_symbol_table_find(&table, "mu") == (void *)3);
   CHECK(akeno_symbol_table_find(&table, "zeta") == (void *)1);
   CHECK(akeno_symbol_table_find(&table, "omega") == NULL);
   akeno_symbol duplicate[] = {{"a", (void *)1}, {"a", (void *)2}};
   akeno_symbol_table bad = {"libbad.so", duplicate, 2};
   CHECK(!akeno_symbol_table_seal(&bad));
   CHECK(akeno_symbol_table_seal(&host_table));
}

static void
test_status_names(void)
{
   for (int i = 0; i < AKENO_ELF_STATUS_COUNT; i++) {
      const char *name = akeno_elf_status_name((akeno_elf_status)i);
      CHECK(name != NULL && strcmp(name, "E_UNKNOWN") != 0);
      for (int j = 0; j < i; j++)
         CHECK(strcmp(name, akeno_elf_status_name((akeno_elf_status)j)) != 0);
   }
   CHECK_EQ_STR(akeno_elf_status_name((akeno_elf_status)999), "E_UNKNOWN");
}

static const akeno_test_case cases[] = {
   {"symbol_tables", test_symbol_tables},
   {"status_names", test_status_names},
   {"parse_summary", test_parse_summary},
   {"imports_listed_before_loading", test_imports_listed_before_loading},
   {"load_rela_4k", test_load_rela_4k},
   {"load_rela_16k", test_load_rela_16k},
   {"load_aps2_4k", test_load_aps2_4k},
   {"load_aps2_16k", test_load_aps2_16k},
   {"load_relr", test_load_relr},
   {"load_android_relr", test_load_android_relr},
   {"load_sysv_hash", test_load_sysv_hash},
   {"load_both_hashes", test_load_both_hashes},
   {"load_console_pages", test_load_console_pages},
   {"load_4k_library_on_console_pages", test_load_4k_library_on_console_pages},
   {"dependency_resolution", test_dependency_resolution},
   {"ifunc", test_ifunc},
   {"tls", test_tls},
   {"missing_symbols_all_named", test_missing_symbols_all_named},
   {"weak_import_binds_null", test_weak_import_binds_null},
   {"android_stack_protector_on_host", test_android_stack_protector_on_host},
   {"out_of_memory", test_out_of_memory},
   {"cleanup_balances", test_cleanup_balances},
   {"invalid_headers", test_invalid_headers},
   {"invalid_segments", test_invalid_segments},
   {"invalid_dynamic", test_invalid_dynamic},
   {"malformed_relocations", test_malformed_relocations},
   {"truncation_sweep", test_truncation_sweep},
   {"mutation_sweep", test_mutation_sweep},
   {"fuzzer_regressions", test_fuzzer_regressions},
};

AKENO_TEST_MAIN(cases)
