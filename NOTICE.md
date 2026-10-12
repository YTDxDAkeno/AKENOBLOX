# Notices

AkenoBlox PS5 is GPL-3.0-or-later. It incorporates, links or is built with the
works below, each under its own licence. "Incorporated" means code is in this
repository or in a built title; "read" means a design was studied and nothing
was copied.

## In this repository

| Work | Licence | Where | How |
|---|---|---|---|
| font8x8 (Daniel Hepper, after Marcel Sondaar and IBM's VGA fonts), `8e279d2` | Public domain | `third_party/font8x8/` | incorporated unmodified |
| PS5_VulkanTemplate (Mihawk, after Sascha Willems' Vulkan examples), `91179df` | MIT | `src/platform/ps5/ps5_pad.c` (pad sample layout, button bits, open with retries), `tools/build/link-ps5-title.sh` (link and packaging steps) | adapted; MIT notice below |
| PS5_Vulkan (Mihawk-99, BlackBearReloaded), `252bf91` | GPL-3.0-or-later | `src/graphics/akeno_tiling.c` (the tile swizzle of `src/demo_renderer.cpp`), `src/platform/ps5/ps5_display.c` (its VideoOut sequence) | adapted |
| AOSP bionic (as pinned by Cordial) | Apache-2.0 | `src/compat/elf/elf_internal.h` | ELF and Android dynamic-tag constants; no code |

## In a built title (`dist/PPSA99256/`)

| Work | Licence | How |
|---|---|---|
| PS5_Vulkan `tooling/native/app_crt.cpp` | GPL-3.0-or-later | compiled and linked as the title's CRT |
| PS5_Vulkan `runtime/libc.prx` (clean-room) | GPL-3.0-or-later | shipped in `sce_module/`; rebuilt from source and checked against PS5_Vulkan's digests |
| PS5_PayloadSDK platform layer (`libps5platform.a`) | GPL-3.0-or-later | linked |
| ps5-payload-dev SDK v0.42 (libc++, libc++abi, libunwind, stubs) | per component (GPL-3.0 tools; LLVM's Apache-2.0 WITH LLVM-exception runtimes) | linked |
| LLVM compiler-rt builtins | Apache-2.0 WITH LLVM-exception | linked |

## Used to build, not shipped

PS5_Vulkan's `ps5-native-tool` and `tools/radv-link.sh` (GPL-3.0-or-later);
zlib 1.3.2 (zlib licence) for the native tool; LLVM/Clang and lld.

## Read for design, nothing copied

Cordial (GPL-3.0-or-later; the runtime design this project ports),
mcpelauncher-linker (MIT; probed, not yet incorporated), libjnivm (MIT; not
yet incorporated), prospero-win (LGPL-2.1-or-later), Playden PS5 (GPL-3.0),
minecraft-linux's libc-shim (no licence stated: deliberately not copied from).

## MIT notice for PS5_VulkanTemplate

```
The MIT License (MIT)

Copyright (c) 2016 Sascha Willems
Copyright (C) 2026 Mihawk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Roblox and Sony

No Roblox binary, asset, APK or decompiled material is in this repository or in
any release, and none ever will be (ADR-0007). No Sony SDK, firmware, keys or
system modules are included. Roblox is a trademark of Roblox Corporation.
PlayStation and PS5 are trademarks of Sony Interactive Entertainment Inc.
Neither company is affiliated with or endorses this project.
