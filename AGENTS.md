# AGENTS.md

For any coding agent working here. Human contributors: the same rules, at more
length, are in [CONTRIBUTING.md](CONTRIBUTING.md).

AkenoBlox PS5 ports Cordial's way of running Roblox's official Android x86-64
engine onto a jailbroken PS5, as a native homebrew title. It does not run
Roblox yet.

## Start every session here

1. Read [docs/PROJECT_STATE.md](docs/PROJECT_STATE.md): phase, last good
   commit, what is verified where, blockers, next tasks.
2. `git log --oneline -20` and `git status`.
3. Check open issues and pull requests.
4. Build and test before changing anything:
   `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build`.
5. Continue from the code as it is. Do not restart the project.

Before ending a session, update PROJECT_STATE.md, TEST_REPORTS.md (what ran,
where, on which commit) and, for any decision, DECISIONS.md.

## The rules

**Verify by running. Never report what you have not observed.** A green build
is a build; a host test is a host test; only a console run is a console run.
Use the vocabulary of [docs/TEST_REPORTS.md](docs/TEST_REPORTS.md): HOST
VERIFIED, CROSS-BUILD VERIFIED, HARDWARE VERIFIED, NOT TESTED, FAILED,
BLOCKED. Label what you reasoned rather than measured **INFERRED**.

**Never fabricate a console result**, a screenshot, a benchmark, a login or a
frame rate. The user runs console tests; ask for the log
([docs/HARDWARE_TESTING.md](docs/HARDWARE_TESTING.md)) and say exactly what is
missing.

**Never make a stub lie.** An unimplemented import returns failure or is
missing by name; it never reports success (Cordial's rule, adopted). A fake
success sends the engine off on an answer that is not true.

**No Roblox code, ever.** No APK, `libroblox.so`, asset or decompiled material
in the repository, an issue, a test or a release. Observing a running engine
is fine; transcribing how it implements something is not.

**No evasion.** Never patch, hook or inject into the engine; never spoof
device integrity, forge tokens or rewrite its traffic; never hide what
AkenoBlox is. If Roblox refuses the runtime, record the blocker
([ADR-0008](docs/DECISIONS.md)).

**No streaming substitutes.** The goal is the engine running on the console.
Remote Play, Moonlight, cloud streaming or a PC doing the work are not
alternatives, whatever the difficulty.

**Console calls: exported functions only**, in shapes a console run has used,
with the source named beside the declaration. Never firmware offsets, never
anything taken from firmware. A libc gap goes to the platform layer recipe,
not a per-call hack (PS5_VulkanTemplate `runtime-surface.md`).

**Licences.** Read a source's licence before copying anything from it; record
what you incorporate in [NOTICE.md](NOTICE.md). A repository with no licence
is read, never copied.

## Where things are

| Need | Look at |
|---|---|
| Why the architecture is what it is | `docs/DECISIONS.md`, `docs/FEASIBILITY.md` |
| What bionic and the console disagree on | `docs/PORTING_NOTES.md` |
| The platform contract | `src/platform/akeno_platform.h`, `akeno_mem.h` |
| The loader | `src/compat/elf/` and `tests/unit/test_elf.c` |
| Console facts (memory, exec, pad, VideoOut, exit) | PS5_VulkanTemplate `skills/`, PS5_PayloadSDK `platform/docs/PROBE.md` |
| What the engine asks of Android | Cordial `docs/` and `docs/analysis/` |
| Reproducing the feasibility experiments | `tools/diagnostics/` |

## Style

C11, three-space indents, 100 columns ([.clang-format](.clang-format)).
Comments say *why*, anchored in the measurement or failure that motivated the
code, with the source named. Commit messages are short: what changed, what was
measured, what is still unverified. Reasoning belongs in an ADR. Documentation
changes with the code it describes, in the same commit; document what exists,
never what is planned.

## Build and test

```bash
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build
cmake -S . -B build-asan -G Ninja -DCMAKE_C_COMPILER=clang -DAKENO_SANITIZE=ON   # then the same
tools/build/build-ps5.sh --no-zip                                                # the title
```

All three must pass before a commit that touches code. Fuzz the loader after
changing it (`-DAKENO_FUZZ=ON`, docs/BUILDING.md); add any crash it finds to
`tests/fixtures/regress/` and `test_fuzzer_regressions`.
