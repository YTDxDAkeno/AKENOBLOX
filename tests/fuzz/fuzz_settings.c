/*
 * AkenoBlox PS5 - libFuzzer target: settings text and log redaction.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "app/akeno_log.h"
#include "app/akeno_settings.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
   char *text = malloc(size + 1);
   if (text == NULL)
      return 0;
   memcpy(text, data, size);
   text[size] = '\0';
   akeno_settings s;
   akeno_settings_defaults(&s);
   akeno_settings_parse(&s, text);
   char out[1024];
   (void)akeno_settings_format(&s, out, sizeof(out));
   char line[512];
   const size_t n = size < sizeof(line) - 1 ? size : sizeof(line) - 1;
   memcpy(line, data, n);
   line[n] = '\0';
   akeno_log_redact(line, sizeof(line));
   free(text);
   return 0;
}
