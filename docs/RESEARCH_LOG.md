# Research log

Sources read, at which revision, and what was learned from each. Newest
entries first. A finding that turns out wrong is corrected here, not deleted.

## 2026-10-12 — session 1

### Revisions read

| Project | Revision | Date | Licence | Role here |
|---|---|---|---|---|
| [luohoa97/cordial](https://github.com/luohoa97/cordial) | `7abbaf5d5f6cc81ca97e448812985d3f180b2420` | 2026-10-08 (0.27.0) | GPL-3.0-or-later (`cordial-protocol` MIT/Apache-2.0) | the runtime design to port |
| ↳ minecraft-linux/mcpelauncher-linker | `1ac3ea6c1cf4f73a84d73f0bff510fba94a7d0f2` | 2026-02-14 | MIT | the AOSP linker port; probed for the console |
| ↳ ↳ minecraft-linux/android_bionic | `081b55b1f45745b1e6f93c49b3831107542a426d` | | Apache-2.0 / BSD | bionic layouts and tag values |
| ↳ ↳ minecraft-linux/android_core | `0235714fbf5593df145e8f991f82c5926c2df2df` | | Apache-2.0 | |
| ↳ ChristopherHX/libjnivm | `f24b98c198fcc5c59a68d1efadd3f5791eb01c2e` | | MIT | the JNI VM to port |
| [mihawk-99/PS5_VulkanTemplate](https://github.com/mihawk-99/PS5_VulkanTemplate) | `91179df760df1e6c6f2d4d172bc435319d79ff3c` | 2026-10-10 | MIT (UI module GPL-3.0-or-later) | console contracts, pad code, link script |
| [mihawk-99/PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) | `252bf912a745e3566a93b69b098bdc8ec4048837` | 2026-10-10 | GPL-3.0-or-later | CRT, link recipe, native tool, libc.prx, VideoOut demo |
| [mihawk-99/PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK) | `611893fc25ef3e138f5bc2c90d829d8b58174cd2` (pinned; HEAD was `b5efad5`) | | GPL-3.0-or-later | platform layer |
| [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk) release v0.42 | zip sha256 `8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da` | | GPL-3.0 (tools) | the toolchain |
| [mpereiraesaa/prospero-win](https://github.com/mpereiraesaa/prospero-win) | `644e32e400c555e340d507383398c5d21b39b0b0` | 2026-10-10 | LGPL-2.1-or-later | console compatibility engineering, read only |
| [leolobato/playden-ps5](https://github.com/leolobato/playden-ps5) | `572fc827f347bc9eb24b2bbea0cfa644d21282bd` | 2026-10-12 | GPL-3.0 | packaging and kit model, read only |
| [dhepper/font8x8](https://github.com/dhepper/font8x8) | `8e279d2d864e79128e96188a6b9526cfa3fbfef9` | 2019-07-08 | public domain | the launcher's font |
| minecraft-linux/libc-shim | `4ed0e447a38250435ec3c4fb596a53491465341c` | 2026-02-25 | **none stated** (GitHub licence field empty) | read for which areas need translation; nothing copied |

### Observations

**Cordial.** Loads `libroblox.so` with the AOSP linker and answers ~650
imports as "cordial", "host" (glibc) or honest stubs (`docs/architecture.md`).
The engine imports zero Vulkan symbols, `dlopen`s `libvulkan.so.1` and asks
for `VK_KHR_android_surface` (`src/android/vulkan.rs`). `libroblox.so` has no
TEXTREL and no plain `.rela.dyn`, only 546 `.rela.plt` entries (patches
README, 0001). Sign-in works through Quick Sign-in; web views (captcha,
Marketplace) are WebKitGTK windows. Roblox bans third-party-client accounts
in waves (README). The runtime crate depends on GTK4, libadwaita, zbus and
Wayland directly (`Cargo.toml`). Its `AGENTS.md` rules — verify by running,
never make a stub lie, no Roblox code ever — are adopted here.

**mcpelauncher-linker.** `include/compat.h` already has `__APPLE__` and
`__FreeBSD__` branches; `linker_phdr.cpp`'s Apple-arm64 path reserves one RWX
`MAP_JIT` region and copies segments into it on 16 KiB pages. Cross-compiled
for the console: 18 errors in 10 translation units, four kinds
([FEASIBILITY.md](FEASIBILITY.md) §3.4).

**PS5_VulkanTemplate.** The console's contracts in one place
(`skills/`): no `dlopen` of a title's own code; execute only via
`sceKernelMprotect` after a read-write mapping in direct memory; 16 KiB pages;
emulated TLS; FTZ/DAZ MXCSR at start; exit through
`sceSystemServiceLoadExec("exit")`; the splash hides a title until dismissed;
pad sample layout and button bits; FTP quirks; storage rules.

**PS5_Vulkan.** RADV with a PS5 winsys, Vulkan 1.4, full CTS run with 0
failures on 2026-09-29; no ETC2/ASTC on GFX10.3. `src/demo_renderer.cpp`
draws CPU frames to VideoOut: type-3 direct memory mapped 0x33, pixel format
`0x8000000022000000`, tiling 0 with an XOR swizzle, flip mode 1. The native
tool and `libc.prx` rebuild bit-for-bit from source (both digests matched
here).

**prospero-win.** Wine's PE loader runs on the console because direct memory
is mapped read-write and then protected; file-backed private mappings are
refused (`ENODEV`) and read into memory instead; signal context at
`ucontext`+64, FP at +320; `FS` stays the libc TLS base; only signed PRX
modules load; `libkernel_sys`-only imports are null in a title. One game per
process, LoadExec between launcher and game.

**Playden.** Each game is a separate title with its own home-screen tile
(ShadowMountPlus), a runtime "kit" checked for completeness before use, and a
sign-in token stored **unencrypted** on the console — the pattern this project
must not repeat for Roblox sessions.

**PS5_PayloadSDK fork.** `exec.h` (`PS5_EXEC_TOGGLED`, `ps5_exec_protect`),
`heap.h` (direct-memory malloc via `--wrap`), `libc.h` (the gaps), `klog.h`,
`memory.h`, `context.h`, `kernel.h` (`sceKernelGetSystemSwVersion`).

### Experiments run

| Experiment | Command | Result |
|---|---|---|
| Toolchain | `prospero-clang -c` of C and C++17 | objects built |
| AOSP linker for the console | `tools/diagnostics/probe-bionic-linker-ps5.sh` | 10 TUs, 18 errors |
| Rust std for FreeBSD | `tools/diagnostics/probe-rust-ps5.sh` | links with 2 missing; native TLS relocations |
| `long double` | clang `__LDBL_MANT_DIG__` per target | Android x86-64 113, PS5 64 |
| pthread sizes | `sizeof` with `prospero-clang` | mutex/cond/rwlock/attr 8, once 16, sem 16, key 4 |
| SDK stub exports | `llvm-nm -D` over `target/lib/*.so` | `kqueue`, `kevent`, `pthread_attr_get_np`, `sigaltstack` exported; `epoll_*`, `recvmmsg`, `sendmmsg` not |
| Native tool and libc.prx | PS5_Vulkan `tools/rebuild-libc.sh` | both digests match |

### Corrections

None yet.
