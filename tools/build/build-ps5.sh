#!/usr/bin/env bash
# AkenoBlox PS5 - build the console title and its release ZIP.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   tools/build/build-ps5.sh [--deps DIR] [--no-zip]
#
# 1. tools/build/setup-ps5-deps.sh: the pinned SDK, PS5_Vulkan, native tool
#    and libc.prx (under .deps unless --deps names another directory);
# 2. CMake with cmake/ps5-toolchain.cmake into build-ps5/: the libraries, the
#    title's objects, then the `title` target, which links, signs and
#    assembles dist/PPSA99256/;
# 3. tools/build/package-release.sh: the ZIP in release/.
# A successful build is a CROSS-BUILD, not a console run (docs/TEST_REPORTS.md).
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
deps="$root/.deps"
zip=true
while (($#)); do
    case $1 in
        --deps) deps=$2; shift 2 ;;
        --no-zip) zip=false; shift ;;
        *) echo "usage: ${0##*/} [--deps DIR] [--no-zip]" >&2; exit 2 ;;
    esac
done

sdk=$(bash "$root/tools/build/setup-ps5-deps.sh" "$deps" | tail -n 1)
deps=$(cd "$deps" && pwd)
export PS5_PAYLOAD_SDK="$sdk"
cmake -S "$root" -B "$root/build-ps5" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$root/cmake/ps5-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAKENO_PS5_VULKAN_DIR="$deps/PS5_Vulkan" \
    -DAKENO_DIST_DIR="$root/dist"
cmake --build "$root/build-ps5"
if $zip; then
    bash "$root/tools/build/package-release.sh" "$root/dist/PPSA99256" "$deps/PS5_Vulkan"
fi
