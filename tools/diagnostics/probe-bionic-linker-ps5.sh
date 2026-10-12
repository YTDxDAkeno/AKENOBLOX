#!/usr/bin/env bash
# AkenoBlox PS5 - the experiment behind docs/FEASIBILITY.md section 3.4:
# cross-compile Cordial's AOSP bionic linker (minecraft-linux's
# mcpelauncher-linker, at the revisions Cordial pins) for the console, and
# summarise what refuses to build. Read-only: nothing is patched.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   probe-bionic-linker-ps5.sh <payload SDK> [work dir]
set -euo pipefail
sdk=$(cd "$1" && pwd)
work=${2:-$(mktemp -d)}
mkdir -p "$work"
linker_revision=1ac3ea6c1cf4f73a84d73f0bff510fba94a7d0f2   # cordial 7abbaf5's submodule pin
if [[ ! -d $work/mcpelauncher-linker/.git ]]; then
    git clone -q https://github.com/minecraft-linux/mcpelauncher-linker "$work/mcpelauncher-linker"
fi
git -C "$work/mcpelauncher-linker" -c advice.detachedHead=false checkout -q "$linker_revision"
git -C "$work/mcpelauncher-linker" submodule update --init --depth 1 -q
cat > "$work/toolchain.cmake" <<TC
set(CMAKE_SYSTEM_NAME FreeBSD)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "$sdk/bin/prospero-clang")
set(CMAKE_CXX_COMPILER "$sdk/bin/prospero-clang++")
set(CMAKE_AR "$sdk/bin/prospero-ar")
set(CMAKE_RANLIB "$sdk/bin/prospero-ranlib")
set(CMAKE_FIND_ROOT_PATH "$sdk")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
TC
cmake -G Ninja -S "$work/mcpelauncher-linker" -B "$work/build" \
    -DCMAKE_TOOLCHAIN_FILE="$work/toolchain.cmake" -DCMAKE_BUILD_TYPE=Release > "$work/configure.log" 2>&1
ninja -C "$work/build" -k 0 > "$work/build.log" 2>&1 || true
echo "translation units that failed: $(grep -c '^FAILED' "$work/build.log" || true)"
echo "errors: $(grep -c 'error:' "$work/build.log" || true)"
grep 'error:' "$work/build.log" | sed 's/.*error: //' | sort | uniq -c | sort -rn
echo "full log: $work/build.log"
