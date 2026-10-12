#!/usr/bin/env bash
# AkenoBlox PS5 - the release ZIP: the title folder and what must ship with it.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   package-release.sh <dist/TITLE_ID> <PS5_Vulkan dir>
#
# Writes release/AkenoBlox-PS5-<version>-<commit>.zip holding the title folder
# (copy it to /data/homebrew/ on the console), the installation guide, the
# changelog, the licence and the notices, and BUILD.txt naming every pinned
# input. No Roblox file is ever included (docs/SECURITY.md, NOTICE.md).
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
title=$1 vulkan=$2
[[ -f $title/eboot.bin && -f $title/sce_sys/param.json ]] || { echo "no title at $title" >&2; exit 2; }
version=$(sed -n 's/^project(AkenoBlox VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")-alpha
commit=$(git -C "$root" rev-parse --short=9 HEAD 2>/dev/null || echo unknown)
if [[ -n $(git -C "$root" status --porcelain --untracked-files=no 2>/dev/null) ]]; then
    commit="$commit-dirty"
fi
name="AkenoBlox-PS5-$version-$commit"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/$name"
cp -r "$title" "$stage/$name/"
cp "$root/LICENSE" "$root/NOTICE.md" "$root/CHANGELOG.md" "$stage/$name/"
cp "$root/docs/INSTALLATION.md" "$root/docs/HARDWARE_TESTING.md" "$stage/$name/"
# Find any Roblox-looking payload before it can leave the machine.
if find "$stage" -iname '*roblox*.so' -o -iname '*.apk' -o -iname '*.xapk' | grep -q .; then
    echo "refusing to package: a Roblox binary or APK is in the title folder" >&2
    exit 1
fi
{
    echo "AkenoBlox PS5 $version ($commit)"
    echo "built: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "status: CROSS-BUILD only; not verified on a console (docs/TEST_REPORTS.md)"
    echo
    echo "eboot.bin sha256:   $(sha256sum "$title/eboot.bin" | cut -d' ' -f1)"
    echo "libc.prx sha256:    $(sha256sum "$title/sce_module/libc.prx" | cut -d' ' -f1)"
    echo "PS5_Vulkan:         $(git -C "$vulkan" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "payload SDK fork:   $(sed -n 's/^sdk_fork_revision=\([0-9a-f]*\).*/\1/p' "$root/tools/build/setup-ps5-deps.sh")"
    echo "payload SDK:        ps5-payload-dev v0.42"
} > "$stage/$name/BUILD.txt"
mkdir -p "$root/release"
rm -f "$root/release/$name.zip"
(cd "$stage" && python3 - "$name" "$root/release/$name.zip" <<'PY'
import os, sys, zipfile
name, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for base, _, files in os.walk(name):
        for f in sorted(files):
            path = os.path.join(base, f)
            info = zipfile.ZipInfo(path, date_time=(2026, 1, 1, 0, 0, 0))
            info.external_attr = 0o644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(path, "rb") as fh:
                z.writestr(info, fh.read())
PY
)
echo "==> release/$name.zip ($(stat -c %s "$root/release/$name.zip") bytes)"
