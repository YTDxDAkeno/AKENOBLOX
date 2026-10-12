# Project state

Read this first in every session; update it last.

**Updated:** 2026-10-12, end of session 1.
**Phase:** M1 and M2 built, awaiting a console run (docs/ROADMAP.md).
**Branch:** `claude/blissful-gauss-9ehj7g`.
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
| Anything on a console | **NOT TESTED** |
| Anything involving Roblox's engine | **NOT TESTED**; nothing loads it yet |

## Open bugs

None known. (One loader bug found by fuzzing this session was fixed and has a
regression test.)

## Blockers and open questions

| Question | Blocks | How to answer |
|---|---|---|
| Does the console execute code a title copied into an `exec.h` region? | M2, everything after | Diagnostics self-test on a console |
| Is `%fs:0x28` stable on the console? | the engine's stack-protected functions | Diagnostics `%fs` probe on a console |
| Which raw system calls does the engine make? | the bionic layer's `syscall` | log them under Cordial on Linux |
| Which Vulkan features does the engine require? | M4 | trace under Cordial on Linux |
| Can a title use UDP? | multiplayer (M6) | a UDP probe in Diagnostics |
| Roblox does not permit third-party clients | legitimate use | policy; not technical. Test accounts only |

## Tracking

One GitHub issue per milestone: M0 [#9](https://github.com/YTDxDAkeno/AKENOBLOX/issues/9)
(closed, done), M1–M8 [#1](https://github.com/YTDxDAkeno/AKENOBLOX/issues/1)–[#8](https://github.com/YTDxDAkeno/AKENOBLOX/issues/8),
each with its acceptance checklist. Tick the boxes there when evidence lands
in TEST_REPORTS.md.

## Next tasks

1. Get a console run of v0.1.0-alpha (HARDWARE_TESTING.md tests 1-2; issues
   #1 and #2). If the self-test fails, record it in FEASIBILITY.md and stop
   Roblox work until it is understood.
2. Under Cordial on Linux, record the engine's `syscall` numbers and Vulkan
   feature requests.
3. Port the AOSP linker (patches in `patches/`), tested against the existing
   fixtures and compared with `akeno_elf`.
4. Start the bionic ABI layer: `pthread_once`, then pthread objects, errno,
   signals, sockets, epoll over kqueue, `long double`.
5. Add a UDP probe to Diagnostics.

## Environment notes for the next session

- `tools/build/setup-ps5-deps.sh` needs network access to GitHub;
  `zlib.net` may be blocked, and the script falls back to GitHub's copy of
  the same tarball (hash-checked).
- Clang's builtins archive must be installed (`libclang-rt-18-dev` on Ubuntu).
- Building RADV (for the engine, M4) is not set up: PS5_Vulkan's
  `tools/build-radv.sh` needs PS5_Mesa plus host LLVM, Clang, libclc and the
  SPIR-V translator.
