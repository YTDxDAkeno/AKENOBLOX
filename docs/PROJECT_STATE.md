# Project state

Read this first in every session; update it last.

**Updated:** 2026-10-12, first real PS5 logs analyzed.
**Phase:** M2 loader hardware gate PASSED on firmware 13.09; M1 launcher partially hardware-verified (exit missing); M3 linker port next.
**Branch:** `chatgpt/udp-loopback-diagnostics` (Draft PR #10 targeting `claude/blissful-gauss-9ehj7g`).
**Last good commit:** `764c088` (code), verified from a fresh clone: host
tests pass, the title cross-builds, `eboot.bin` sha256 `5a96d735…3877`
([TEST_REPORTS.md](TEST_REPORTS.md)). Later commits on the branch are
documentation.

## What exists

- A native PS5 title, `PPSA99256`, that builds, signs and packages: launcher
  with Home, Start Roblox, Runtime Status, Diagnostics, Controller (input
  test), Graphics & Audio, About, Exit; DualSense; CPU-drawn VideoOut frames;
  klog; settings and logs in storage; the shell exit.
- `akeno_elf`, a loader for Android x86-64 shared objects: RELA, APS2, RELR,
  IFUNC on request, constructors, symbol lookup, missing-import reports;
  platform memory (`ps5platform/exec.h` on the console).
- The Diagnostics probes: CPUID and x86-64 level, `%fs:0x28`, the ELF self-test
  over embedded fixtures, storage, DNS; `diagnostics.txt`.
- A package check for a user-supplied `libroblox.so` (validates, never runs).
- DualSense-to-Android gamepad mapping.
- Host build, 9 test suites, sanitizers, 3 fuzz targets, CI for host and
  cross-build, the feasibility experiments as scripts.

## Verified

| What | Level |
|---|---|
| Everything above, on a host | HOST VERIFIED |
| The title | CROSS-BUILD VERIFIED |
| ELF loader synthetic fixture, 13 tests; CPU, VideoOut, pad, storage on firmware 13.09 | **HARDWARE VERIFIED**: original build `80a3f11f9`, user-provided logs |
| Anything involving Roblox's engine | **NOT TESTED**; no `libroblox.so` installed |

## Open bugs

None known. (One loader bug found by fuzzing this session was fixed and has a
regression test.)

## Blockers and open questions

| Question | Blocks | How to answer |
|---|---|---|
| Does the console execute code a title copied into an `exec.h` region? | **RESOLVED on firmware 13.09 for synthetic fixtures** | 13/13 PS5 self-test, four runs, logs in TEST_REPORTS.md |
| Is `%fs:0x28` stable and properly seeded on the console? | engine stack-protected functions | Stable in both threads, but value **zero**; requires further ABI/guard analysis |
| Which raw system calls does the engine make? | the bionic layer's `syscall` | log them under Cordial on Linux |
| Which Vulkan features does the engine require? | M4 | trace under Cordial on Linux |
| Can a title use UDP? | multiplayer (M6) | local-loopback probe in PR #10; must hardware-test; external UDP still unverified |
| Why does `www.roblox.com` fail DNS resolution? | external services/authentication | reproduce DNS check; review PS5 DNS configuration and platform resolver (error 4) |
| Roblox does not permit third-party clients | legitimate use | policy; not technical. Test accounts only |

## Tracking

One GitHub issue per milestone: M0 [#9](https://github.com/YTDxDAkeno/AKENOBLOX/issues/9)
(closed, done), M1–M8 [#1](https://github.com/YTDxDAkeno/AKENOBLOX/issues/1)–[#8](https://github.com/YTDxDAkeno/AKENOBLOX/issues/8),
each with its acceptance checklist. Tick the boxes there when evidence lands
in TEST_REPORTS.md.

## Next tasks

1. Record and preserve the successful M2 console evidence (TEST_REPORTS.md) and close M2 issue only after reviewing the recorded result; verify clean shell exit for M1 separately.
2. Under Cordial on Linux, record the engine's `syscall` numbers and Vulkan
   feature requests.
3. Port the AOSP linker (patches in `patches/`), tested against the existing
   fixtures and compared with `akeno_elf`.
4. Start the bionic ABI layer: `pthread_once`, then pthread objects, errno,
   signals, sockets, epoll over kqueue, `long double`.
5. Hardware-test PR #10's local-only UDP probe; investigate DNS failure and zero stack canary; do not infer real multiplayer connectivity.

## Environment notes for the next session

- `tools/build/setup-ps5-deps.sh` needs network access to GitHub;
  `zlib.net` may be blocked, and the script falls back to GitHub's copy of
  the same tarball (hash-checked).
- Clang's builtins archive must be installed (`libclang-rt-18-dev` on Ubuntu).
- Building RADV (for the engine, M4) is not set up: PS5_Vulkan's
  `tools/build-radv.sh` needs PS5_Mesa plus host LLVM, Clang, libclc and the
  SPIR-V translator.
