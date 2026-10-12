# Architecture

A map of the tree as it stands, and of where it is going. Where this page and
an ADR in [DECISIONS.md](DECISIONS.md) disagree, the ADR wins and this page
is out of date.

```text
  PS5 home screen
        |  launches PPSA99256 from /data/homebrew (ShadowMountPlus or similar)
        v
+------------------------------------------------------------------+
| 1  AkenoBlox launcher (this program)               src/app, src/ui |
|    menu, settings, diagnostics, self-test, package check           |
+------------------------------------------------------------------+
| 2  Platform layer                    src/platform/akeno_platform.h |
|    host: src/platform/host      console: src/platform/ps5          |
|    code memory, time, log, storage, pad, display, DNS, exit        |
+------------------------------------------------------------------+
        |  "Start Roblox" -> sceSystemServiceLoadExec (ADR-0004)  [planned]
        v
+------------------------------------------------------------------+
| 3  Android compatibility runtime                         [planned] |
|    AOSP bionic linker (port)   bionic ABI on the console's libc    |
|    libjnivm                    Android framework (from Cordial)    |
|    akeno_elf (today: self-test, validator)        src/compat/elf   |
+------------------------------------------------------------------+
| 4  Graphics bridge                                       [planned] |
|    virtual libvulkan -> RADV (linked) -> VK_KHR_display -> VideoOut |
|    VK_KHR_android_surface translated; ETC2 decoded on the CPU      |
+------------------------------------------------------------------+
| 5  libroblox.so (user-supplied, unmodified)                        |
+------------------------------------------------------------------+
| 6  Roblox's services over the network (the engine's own cURL/UDP)  |
+------------------------------------------------------------------+
```

## What exists

| Path | What it is | Verified |
|---|---|---|
| `src/compat/elf/` | `akeno_elf`: parse and validate ELF64 x86-64 shared objects; copy, relocate (RELA, APS2, RELR), protect, IFUNC on request, constructors, symbol lookup, symbol tables and resolver chains | host |
| `src/platform/akeno_platform.h`, `akeno_mem.h` | the platform contract | — |
| `src/platform/host/` | POSIX implementation; scripted input and PPM frames for tests | host |
| `src/platform/ps5/` | console implementation: klog, splash, user service, DualSense, VideoOut CPU presentation, `ps5platform/exec.h` code memory, storage, DNS, the shell exit | cross-build |
| `src/app/` | the launcher: start-up, frame loop, commands; menu state machine; settings; log with credential redaction | host |
| `src/ui/` | software canvas, font, the screens | host |
| `src/graphics/akeno_tiling.c` | VideoOut's 64 KiB tile layout and its inverse | host |
| `src/input/` | menu edges and repeat; DualSense to Android gamepad (KeyEvent keycodes, MotionEvent axes) | host |
| `src/diagnostics/` | CPUID and x86-64 level, the `%fs:0x28` probe, the ELF self-test over embedded fixtures, the diagnostics report | host |
| `src/runtime/` | the runtime's stage model; the user-supplied package check | host |
| `tests/` | unit tests, fixtures, libFuzzer targets, the host journey | — |
| `tools/build/` | PS5 dependencies, link and signing, release ZIP, icon | cross-build |
| `tools/deploy/` | FTP upload and klog capture for a console | not yet run against one |
| `tools/diagnostics/` | the feasibility experiments, reproducible | host |

## Interfaces between the layers

**Launcher → platform** (`akeno_platform.h`). Lifecycle (`init`, `shutdown`,
`exit`), facts (`query`), code memory, time, a log line sink, storage roots,
one pad snapshot per frame in platform-neutral bits, a display that presents a
linear 0xAARRGGBB frame, and a DNS check. Console constants never cross it.

**Loader → platform** (`akeno_mem.h`). `reserve` gives zero-filled,
read-write memory at an alignment; `protect` applies a protection to page
ranges and reports what was applied (the console only toggles read-write and
read-execute); `release` gives it back. That is all a loader needs, and it is
what the AOSP linker port's copy-load path will use too.

**Runtime → engine** (planned). Imports answered by symbol tables
(`akeno_symbol_table`), one per Android library the engine needs; every
import either has a real implementation or fails by name — no stub reports
success (Cordial AGENTS.md, "Never make a stub lie").

## Data on the console

| Where | What |
|---|---|
| `/app0` (`/data/homebrew/PPSA99256`) | the title: `eboot.bin`, `sce_sys/`, `sce_module/libc.prx`; read-only while running |
| `/data/akenoblox` or `/download0/akenoblox` | settings, logs, `diagnostics.txt`, and later the user's Roblox files under `roblox/` |

## Process model

One process per program, handed over with `sceSystemServiceLoadExec`
(ADR-0004). Today only the launcher exists.
