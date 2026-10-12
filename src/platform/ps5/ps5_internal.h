/*
 * AkenoBlox PS5 - the console's exported functions this platform calls, and
 * the pieces of the PS5 platform shared between its files.
 * Copyright (C) 2026 AkenoBlox contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The payload SDK links these (its stub libraries name them) but declares
 * few. Each declaration is the shape a console run has used: PS5_Vulkan's
 * src/demo_renderer.cpp for VideoOut and direct memory, PS5_VulkanTemplate's
 * ps5/src/platform.c for the pad, the user service, the splash and LoadExec.
 * Exported functions only; nothing here reaches into the firmware.
 */
#ifndef AKENO_PS5_INTERNAL_H
#define AKENO_PS5_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "akeno_platform.h"

int sceSystemServiceHideSplashScreen(void);
int sceSystemServiceLoadExec(const char *path, const char *const *argv);
int sceUserServiceInitialize(const void *params);
int sceUserServiceGetInitialUser(int32_t *user_id);
int scePadInit(void);
int scePadOpen(int32_t user_id, int32_t port_type, int32_t index, const void *params);
int scePadGetHandle(int32_t user_id, int32_t port_type, int32_t index);
int scePadRead(int32_t handle, void *samples, int32_t capacity);
int scePadClose(int32_t handle);
int sceKernelUsleep(uint32_t microseconds);

/* The pad (ps5_pad.c). */
bool akeno_ps5_pad_open(void);
void akeno_ps5_pad_close(void);

/* Code memory (ps5_memory.c). */
const akeno_mem_ops *akeno_ps5_code_memory(void);

#endif /* AKENO_PS5_INTERNAL_H */
