# Test reports

What ran, where, on which build, and what it showed. Newest first. Pass or
fail, at the same level of detail.

## Vocabulary

| Level | Means |
|---|---|
| **HOST VERIFIED** | ran and passed on a Linux x86-64 host |
| **CROSS-BUILD VERIFIED** | compiled, linked, converted and signed for the console; nothing ran |
| **HARDWARE VERIFIED** | ran and passed on a real PS5, with the log recorded here |
| **NOT TESTED** | not run anywhere |
| **FAILED** | ran and failed |
| **BLOCKED** | cannot proceed for a recorded reason |

A green CI run is never evidence of console behaviour, and nothing here is
evidence that Roblox runs anywhere: it does not, yet.

## Summary by feature

| Feature | Level |
|---|---|
| ELF loader: parse, copy, relocate (RELA, APS2, RELR), protect, symbols | HOST VERIFIED |
| ELF loader at 16 KiB pages | HOST VERIFIED (emulated page size) |
| ELF loader on the console's executable memory | **HARDWARE VERIFIED** on 13.09 with 13/13 fixture probes |
| ELF loader against malformed input (sweeps and libFuzzer) | HOST VERIFIED |
| Launcher menus, settings, input mapping, rendering | HOST VERIFIED |
| Launcher on the console (display, pad, storage, exit) | Display, pad, storage **HARDWARE VERIFIED** on 13.09; clean exit not documented |
| Title packaging (`eboot.bin`, `sce_sys`, `libc.prx`) | CROSS-BUILD VERIFIED |
| AOSP linker for the console | FAILED to compile: 18 errors, understood (FEASIBILITY §3.4) |
| Rust std for the console | links; native TLS model incompatible (FEASIBILITY §3.6) |
| Everything Roblox | NOT TESTED |

## 2026-10-12 — first real PS5 hardware report, build `80a3f11f9`

**Evidence:** User supplied `diagnostics.txt` and `akenoblox.log` from an actual
PS5. Results were reviewed on 2026-10-12. Firmware **13.090.001**
(`0x13090001`), console title `PPSA99256`, SDK v0.42.
**These results apply to the original `80a3f11f9` build, not the later
`chatgpt/udp-loopback-diagnostics` branch.** Full logs were provided
privately for review; this summary avoids distributing raw machine data.

| Feature | Result / evidence |
|---|---|
| Launcher starts and displays 1920x1080 via VideoOut | **HARDWARE VERIFIED**: report row `Display ok`, runtime log `display: 1920x1080` |
| DualSense | **HARDWARE VERIFIED**: `Controller connected` at startup |
| Writable storage | **HARDWARE VERIFIED**: `/download0/akenoblox`; saved `diagnostics.txt` |
| CPU floor | **HARDWARE VERIFIED**: x86-64-v3; AVX enabled in XCR0 |
| ELF loader | **HARDWARE VERIFIED** for the embedded synthetic Android x86-64 fixtures: **13/13 passed**, run four times (712, 670, 669, 713 microseconds) |
| Relocation and executable memory | **HARDWARE VERIFIED**: APS2 / RELR, 16 KiB pages, W^X (0 W+X pages), constructors/destructors, PLT, GLOB_DAT, loaded code called |
| Stack-protector test | **PASS for the fixture**, but **ABI completeness NOT VERIFIED**: `%fs:0x28` was stable on both threads yet read as **zero**; investigate actual guard initialization, randomness and consistency with Android |
| DNS | **FAILED/WARN**: `getaddrinfo("www.roblox.com", "443")` returned 4; no confirmed external DNS or Internet. The console uses deliberate PSN-blocking DNS `62.210.38.117` (user clarified after the original report). Cause unconfirmed; branch adds neutral `example.com` control without altering DNS |
| UDP | **NOT TESTED** in this build; a local-only probe exists separately in PR #10, awaiting a console run |
| Normal exit to PS5 Home | **NOT VERIFIED**: user log contains no exit event |
| Real Roblox library loading / game rendering | **NOT TESTED**: `libroblox.so` was not installed on the console |

The `ELF loader` self-test gives positive evidence that this firmware can
execute the loaded synthetic x86-64 fixture. It does **not** establish that
the full AOSP linker, Android libc ABI, Vulkan surface translation, network
services, or Roblox authentication work.

**M2 gate:** passed for the tested loader subset; proceed with the Android
linker port, subject to the distinct ABI and network blockers. Do not expand
the result to other firmwares or untested Roblox builds.

## 2026-10-12 — session 1, clean build of `764c088`

A fresh `git clone` of `764c088` (branch `claude/blissful-gauss-9ehj7g`), so
nothing depended on untracked files:

- host: configure, build, `ctest`: **9 of 9 suites pass**; `--version` prints
  `AkenoBlox PS5 0.1.0-alpha (764c08841)`. HOST VERIFIED.
- console: `tools/build/build-ps5.sh` → `dist/PPSA99256/eboot.bin`, 313,033
  bytes, sha256 `5a96d73590c72efad76091c510d630c11b1c409688294a59c7beecaad0bc3877`;
  SELF digest `1542d7c7…2566`, `integrity: valid`; 178 imports, none null in a
  title; `libc.prx` sha256 `e6ff45d1…0036` (PS5_Vulkan's recorded digest);
  `release/AkenoBlox-PS5-0.1.0-alpha-764c08841.zip`, 182,066 bytes, sha256
  `7df325c6dd13cfe45aa6dfd626126c17662380c7dcc7597df3efb0fcb7077279`, 10 files,
  no Roblox file. CROSS-BUILD VERIFIED.
- console run: **NOT TESTED**.

## 2026-10-12 — session 1, host and cross-build

**Environment.** Ubuntu 24.04 container, 4 vCPU, GCC 13.3.0, Clang/lld
18.1.3, CMake 3.28.3, Python 3.13. Payload SDK v0.42 with PS5_PayloadSDK
`611893f`, PS5_Vulkan `252bf91`. **Build:** the working tree that became `764c088`
(its clean rebuild is the entry above).

### Host tests — HOST VERIFIED

`cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build`,
and again with `-DCMAKE_C_COMPILER=clang -DAKENO_SANITIZE=ON` (ASan + UBSan):
**9 of 9 suites pass in both**, no compiler warnings with `-Werror`.

| Suite | Cases | What it covers |
|---|---|---|
| `test_elf` | 29 | 8 fixture variants loaded and called; kernel-checked page permissions; 16 KiB pages; dependency chain; IFUNC (refused, then allowed); TLS refused, emulated TLS named; all missing imports named; weak imports; Android stack protector on glibc; reserve and protect failures; 50 load/unload cycles balanced; invalid headers, segments, dynamic sections, relocations (RELA, APS2, RELR); every prefix of a file; 2,400 mutations; the fuzzer's regression input |
| `test_input` | 7 | edges, D-pad repeat timing, stick hysteresis, default mapping and Android constants, mapping parse and refusal, dead zone rescaling, key edges |
| `test_settings` | 5 | round trip, clamping and refusals, atomic save/load, credential redaction, the log ring |
| `test_menu` | 7 | every journey: home, Start, Diagnostics commands and scrolling, settings saved on leaving, the input test's held exit, exit confirmation |
| `test_canvas` | 6 | clipping, blending, text and ellipsis, rounded rectangles, every screen draws deterministically and distinctly, the selection drawn where the menu says |
| `test_runtime` | 4 | no stage claims hardware verification; package check: missing, not ELF, a real x86-64 object, `DT_NEEDED` names |
| `test_selftest` | 6 | the console self-test passes on host memory at 4 KiB and 16 KiB pages; CPU levels; `%fs:0x28` stable on glibc; report rows |
| `test_tiling` | 3 | the VideoOut swizzle is a bijection; sizes; address-order copy equals the scatter |
| `host_journey` | 1 | the real program driven through every screen by scripted input; settings, report, log and a 1920x1080 frame checked; `--diagnostics` exits 0 |

The host `--diagnostics` report on this machine: x86-64-v4, AVX state saved,
`%fs:0x28` stable on both threads, self-test **PASSED** (13 steps).

### Fuzzing — HOST VERIFIED, one bug found and fixed

Clang 18 libFuzzer with ASan + UBSan, every library instrumented, seeded with
the fixtures:

| Target | Run | Result |
|---|---|---|
| `fuzz_elf_parse` | 90 s, 9,425,374 executions | no failure |
| `fuzz_elf_load` | first run | **SEGV** in `read_mem` after 358,152 executions: a malformed image pointed its hash table at a page left without access; `akeno_elf_symbol` read it. Fixed by checking applied page protection before every post-load read (`mod_readable`); the input is kept as `tests/fixtures/regress/symbol-table-in-unreadable-page.bin` |
| `fuzz_elf_load` | after the fix: 180 s, 1,828,497 executions, coverage 1,354 | no failure |
| `fuzz_settings` | 30 s, 620,406 executions | no failure |

### Static analysis

GCC 13 `-fanalyzer` over every source: no findings.

### PS5 cross-build — CROSS-BUILD VERIFIED

`tools/build/setup-ps5-deps.sh` from an empty directory: SDK v0.42 sha256
verified, platform layer installed, PS5_Vulkan's native tool and `libc.prx`
rebuilt twice with both digests matching its records (raw
`8ee6e124…02bb`, signed `e6ff45d1…0036`), in 34 s.

`tools/build/build-ps5.sh`: every source compiles for `x86_64-sie-ps5` with
`-Werror`; the title links with PS5_Vulkan's CRT and recipe; **no import
defined only by `libkernel_sys` or `libScePosixForWebKit`** (178 imports, all
exported by modules a title loads); `ps5-native-tool self --inspect`:
`container: signed, plaintext`, `integrity: valid`. `dist/PPSA99256/` holds
`eboot.bin`, `sce_sys/param.json`, `sce_sys/icon0.png`, `sce_module/libc.prx`.

### Feasibility experiments

| Experiment | Result |
|---|---|
| `tools/diagnostics/probe-bionic-linker-ps5.sh` | 10 translation units fail, 18 errors (4 kinds), reproduced twice |
| `tools/diagnostics/probe-rust-ps5.sh` | links with `dl_iterate_phdr`, `pthread_setname_np` missing; 37 `TLSGD`, 16 `TLSLD`, 28 `DTPOFF32` relocations |
| type sizes, `long double` | FEASIBILITY §1.5 |

### Not tested

Anything on a console. The tests to run are in
[HARDWARE_TESTING.md](HARDWARE_TESTING.md).
