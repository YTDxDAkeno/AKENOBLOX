#!/usr/bin/env bash
# AkenoBlox PS5 - link the title and assemble its folder.
# Copyright (C) 2026 AkenoBlox contributors
# Adapted from PS5_VulkanTemplate ps5/tools/link-title.sh,
# Copyright (C) 2026 Mihawk, MIT licence (see NOTICE.md).
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   link-ps5-title.sh WORK_DIR PS5_VULKAN_DIR SDK OUT_DIR INPUT...
#
# Run by the PS5 build (CMakeLists.txt, target `title`). The link is
# PS5_Vulkan's, as every title on that foundation is linked: its CRT
# (tooling/native/app_crt.cpp), its link recipe (tools/radv-link.sh: the SDK's
# platform layer, the heap and thread wraps, the libc names the console lacks
# bound to ps5_*), its native tool (ELF to the console's fake SELF) and its
# clean-room libc.prx. The launcher does not link RADV yet, so the recipe is
# given an empty archive where RADV's would go.
set -euo pipefail
[[ -n ${LINK_TRACE:-} ]] && set -x

work=$1 vulkan=$2 sdk=$3 out=$4
shift 4
inputs=("$@")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
tool="$vulkan/build/runtime-shim/ps5-native-tool"
[[ -x $tool ]] || tool="$vulkan/build/host/ps5-native-tool"
native="$vulkan/tooling/native"
param="$root/sce_sys/param.json"
export PS5_CLANG=${PS5_CLANG:-$(command -v clang || true)}

for file in "$tool" "$param" "$sdk/bin/prospero-lld" "$vulkan/tools/radv-link.sh" \
        "$vulkan/runtime/libc.prx" "$native/app_crt.cpp"; do
    [[ -e $file ]] || { echo "missing: $file (run tools/build/setup-ps5-deps.sh)" >&2; exit 2; }
done
mkdir -p "$work/obj"
cc() { PS5_PAYLOAD_SDK="$sdk" sh "$vulkan/tooling/prospero-clang18" "$@"; }

# The CRT: _start, the init and fini arrays, and the catchReturnFromMain hook
# src/platform/ps5/platform_ps5.c defines.
cc -std=c++20 -O2 -fno-exceptions -fno-rtti -c "$native/app_crt.cpp" -o "$work/obj/app_crt.o"

# The recipe, with an empty archive where RADV's goes.
"$sdk/bin/llvm-ar" rcs "$work/obj/libnoradv.a"
# shellcheck source=/dev/null
source "$vulkan/tools/radv-link.sh"
radv_link_recipe "$vulkan" "$sdk" "$work/obj/libnoradv.a" || exit 2

"$sdk/bin/prospero-lld" "${radv_linker_script[@]}" --eh-frame-hdr "${radv_link_flags[@]}" \
    --version-script "$native/app-symbols.map" --exclude-libs=ALL --gc-sections \
    -e _start -o "$work/llvm-pie.elf" \
    "$work/obj/app_crt.o" "${inputs[@]}" \
    "${radv_link_inputs[@]}" \
    --as-needed "$sdk"/target/lib/*.so

# A title loads neither libkernel_sys's exports nor libScePosixForWebKit's: an
# import only their stubs define is null at run time, and its first call jumps
# to address 0 (PS5_VulkanTemplate runtime-surface.md). Refuse it here.
null_imports=$(comm -23 \
    <("$sdk/bin/llvm-nm" -D --undefined-only "$work/llvm-pie.elf" |
        awk '$1 == "U" { sub(/@.*/, "", $2); print $2 }' | sort -u) \
    <(for library in "$sdk"/target/lib/*.so; do
        case ${library##*/} in libkernel_sys.so | libScePosixForWebKit.so) continue ;; esac
        "$sdk/bin/llvm-nm" -D --defined-only "$library" 2>/dev/null | awk '{ print $NF }'
    done | sort -u))
if [[ -n $null_imports ]]; then
    echo "link-ps5-title.sh: imports no module a title loads exports (null at run time): ${null_imports//$'\n'/ }" >&2
    exit 1
fi
"$sdk/bin/llvm-nm" -D --undefined-only "$work/llvm-pie.elf" |
    awk '$1 == "U" { sub(/@.*/, "", $2); print $2 }' | sort -u > "$work/imports.txt"

"$tool" link --in "$work/llvm-pie.elf" --out "$work/eboot.elf.new" \
    --stub-dir "$sdk/target/lib" --module-sdk 0x02000009 --companion-sdk 0x08050001 \
    --file-name eboot.elf

title_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' "$param")
app="$out/$title_id"
rm -rf "$app"
mkdir -p "$app/sce_sys" "$app/sce_module"
"$tool" self --sign --in "$work/eboot.elf.new" --out "$app/eboot.bin" --magic 0x1D3D154F
"$tool" self --inspect --file "$app/eboot.bin" > "$work/eboot.inspect.txt"
cp "$param" "$app/sce_sys/param.json"
python3 "$root/tools/build/make-icon.py" "$app/sce_sys/icon0.png"
(cd "$vulkan/runtime" && sha256sum --check --strict --quiet libc.prx.sha256)
cp "$vulkan/runtime/libc.prx" "$app/sce_module/libc.prx"
mv "$work/eboot.elf.new" "$work/eboot.elf"
printf '==> %s: %s (eboot.bin %s bytes, %s imports)\n' "$title_id" "$app" \
    "$(stat -c %s "$app/eboot.bin")" "$(wc -l < "$work/imports.txt")"
