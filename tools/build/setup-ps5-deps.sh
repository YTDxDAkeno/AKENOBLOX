#!/usr/bin/env bash
# AkenoBlox PS5 - fetch and build everything the console build needs, pinned.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   tools/build/setup-ps5-deps.sh [deps-dir]      (default: .deps)
#
# Installs, each at a pinned revision and checked:
#   <deps>/PS5_PayloadSDK   mihawk-99's payload SDK fork (the platform layer)
#   <deps>/ps5-payload-sdk  the installed SDK: ps5-payload-dev v0.42 release
#                           (sha256-checked) with the fork's platform layer
#   <deps>/PS5_Vulkan       PS5_Vulkan: the CRT, the link recipe, the
#                           ELF-to-SELF native tool and the clean-room
#                           libc.prx, both rebuilt here and checked against
#                           the digests PS5_Vulkan records
# Nothing here touches a console. Prints the SDK path last.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
deps=${1:-$root/.deps}
mkdir -p "$deps"
deps=$(cd "$deps" && pwd)

# The pins. Bump together, with a regression run (docs/BUILDING.md).
sdk_fork_url=https://github.com/mihawk-99/PS5_PayloadSDK.git
sdk_fork_revision=611893fc25ef3e138f5bc2c90d829d8b58174cd2   # PS5_VulkanTemplate's pin
vulkan_url=https://github.com/mihawk-99/PS5_Vulkan.git
vulkan_revision=252bf912a745e3566a93b69b098bdc8ec4048837
zlib_version=1.3.2
zlib_hash=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
# zlib.net is the URL PS5_Vulkan uses; GitHub carries the same tarball, and
# the hash decides, not the host.
zlib_urls=("https://zlib.net/fossils/zlib-$zlib_version.tar.gz"
           "https://github.com/madler/zlib/releases/download/v$zlib_version/zlib-$zlib_version.tar.gz")

for command in git curl unzip sha256sum make clang clang++ flock; do
    command -v "$command" > /dev/null || { echo "missing: $command" >&2; exit 2; }
done

checkout() {
    local url=$1 dir=$2 revision=$3
    if [[ ! -d $dir/.git ]]; then
        git clone -q "$url" "$dir"
    fi
    if ! git -C "$dir" cat-file -e "$revision^{commit}" 2>/dev/null; then
        git -C "$dir" fetch -q origin
    fi
    git -C "$dir" -c advice.detachedHead=false checkout -q "$revision"
}

echo "==> PS5_PayloadSDK at ${sdk_fork_revision:0:12}" >&2
checkout "$sdk_fork_url" "$deps/PS5_PayloadSDK" "$sdk_fork_revision"
sdk="$deps/ps5-payload-sdk"
if [[ ! -f $sdk/.ps5-sdk-revision || $(<"$sdk/.ps5-sdk-revision") != "$sdk_fork_revision" ]]; then
    tree=$(mktemp -d)
    git -C "$deps/PS5_PayloadSDK" archive "$sdk_fork_revision" | tar -x -C "$tree"
    mkdir -p "$deps/cache"
    # The fork's installer downloads the v0.42 release with wget and checks its
    # sha256; curl stands in where wget is missing.
    if ! command -v wget > /dev/null; then
        mkdir -p "$deps/bin"
        printf '#!/bin/sh\n# wget -q URL -O FILE\nexec curl -fsSL "$2" -o "$4"\n' > "$deps/bin/wget"
        chmod +x "$deps/bin/wget"
        export PATH="$deps/bin:$PATH"
    fi
    bash "$tree/platform/tools/setup-sdk.sh" "$sdk" "$sdk_fork_revision" "$deps/cache" >&2
    rm -rf "$tree"
fi
[[ -x $sdk/bin/prospero-lld ]] || { echo "the SDK install is incomplete" >&2; exit 2; }

echo "==> PS5_Vulkan at ${vulkan_revision:0:12}" >&2
checkout "$vulkan_url" "$deps/PS5_Vulkan" "$vulkan_revision"
zlib_dir="$deps/PS5_Vulkan/.deps/native/zlib"
zlib_archive="$zlib_dir/zlib-$zlib_version.tar.gz"
mkdir -p "$zlib_dir"
if ! printf '%s  %s\n' "$zlib_hash" "$zlib_archive" | sha256sum --check --strict > /dev/null 2>&1; then
    for url in "${zlib_urls[@]}"; do
        if curl -fsSL "$url" -o "$zlib_archive.download" 2> /dev/null &&
            printf '%s  %s\n' "$zlib_hash" "$zlib_archive.download" | sha256sum --check --strict > /dev/null; then
            mv "$zlib_archive.download" "$zlib_archive"
            break
        fi
        rm -f "$zlib_archive.download"
    done
fi
printf '%s  %s\n' "$zlib_hash" "$zlib_archive" | sha256sum --check --strict > /dev/null ||
    { echo "zlib $zlib_version could not be fetched with the pinned hash" >&2; exit 2; }
if [[ ! -x $deps/PS5_Vulkan/build/runtime-shim/ps5-native-tool || ! -f $deps/PS5_Vulkan/runtime/libc.prx ]]; then
    # Builds the native tool and libc.prx twice each and installs libc.prx only
    # when both builds match PS5_Vulkan's recorded digests.
    (cd "$deps/PS5_Vulkan" && bash tools/rebuild-libc.sh) >&2
fi
(cd "$deps/PS5_Vulkan/runtime" && sha256sum --check --strict --quiet libc.prx.sha256)

builtins="$(clang --print-resource-dir)/lib/linux/libclang_rt.builtins-x86_64.a"
[[ -f $builtins ]] || {
    echo "clang's builtins are missing ($builtins): install libclang-rt-dev for this clang" >&2
    exit 2
}
echo "$sdk"
