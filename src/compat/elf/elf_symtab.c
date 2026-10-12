/*
 * AkenoBlox PS5 - akeno_elf: host symbol tables and resolver chains.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A table is how a set of functions this program implements is offered to a
 * loaded library under a library's name, the shape Cordial's symtab gives its
 * "virtual libraries". On the console it is the only shape there is: the title
 * cannot gain imports at run time (PS5_VulkanTemplate loading-code.md), so
 * every function an engine may call must already be linked into the title and
 * listed in a table.
 */
#include "elf_internal.h"

#include <stdlib.h>

static int
compare_symbols(const void *a, const void *b)
{
   return strcmp(((const akeno_symbol *)a)->name, ((const akeno_symbol *)b)->name);
}

bool
akeno_symbol_table_seal(akeno_symbol_table *table)
{
   if (table == NULL || (table->symbols == NULL && table->count != 0))
      return false;
   if (table->count < 2)
      return true;
   qsort(table->symbols, table->count, sizeof(*table->symbols), compare_symbols);
   for (size_t i = 1; i < table->count; i++)
      if (strcmp(table->symbols[i - 1].name, table->symbols[i].name) == 0)
         return false;
   return true;
}

void *
akeno_symbol_table_find(const akeno_symbol_table *table, const char *name)
{
   if (table == NULL || name == NULL || table->count == 0)
      return NULL;
   size_t low = 0, high = table->count;
   while (low < high) {
      const size_t mid = low + (high - low) / 2;
      const int order = strcmp(name, table->symbols[mid].name);
      if (order == 0)
         return table->symbols[mid].address;
      if (order < 0)
         high = mid;
      else
         low = mid + 1;
   }
   return NULL;
}

void *
akeno_resolver_chain_resolve(void *chain_ctx, const char *name, const char *version)
{
   (void)version; /* bionic's version names are checked by tests, not used for lookup yet */
   const akeno_resolver_chain *chain = chain_ctx;
   if (chain == NULL || name == NULL)
      return NULL;
   for (size_t i = 0; i < chain->module_count && i < AKENO_RESOLVER_MAX; i++) {
      void *found = akeno_elf_symbol(chain->modules[i], name);
      if (found != NULL)
         return found;
   }
   for (size_t i = 0; i < chain->table_count && i < AKENO_RESOLVER_MAX; i++) {
      void *found = akeno_symbol_table_find(chain->tables[i], name);
      if (found != NULL)
         return found;
   }
   return NULL;
}
