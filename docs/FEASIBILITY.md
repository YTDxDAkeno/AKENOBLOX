# Technical feasibility audit

**Question:** can Cordial's way of running Roblox's official Android x86-64
engine be carried to a jailbroken PS5, as a native homebrew title, without
PSN, streaming or a PC?

**Date:** 2026-10-12. **Audited revisions:** see [RESEARCH_LOG.md](RESEARCH_LOG.md)
(Cordial `7abbaf5`, PS5_VulkanTemplate `91179df`, PS5_Vulkan `252bf91`,
prospero-win `644e32e`, Playden `572fc82`, PS5_PayloadSDK fork `611893f`).

**Hardware update (2026-10-12):** User-provided PS5 logs from firmware
13.09, build `80a3f11f9`, confirm **13/13 synthetic ELF self-test steps
passed** (four consecutive runs), including executing relocated x86-64
code from the console's allocated executable pages. The launcher displayed
1080p frames, recognized DualSense and wrote to `/download0/akenoblox`.
Two distinct issues remain: DNS to `www.roblox.com` failed with resolver
code 4; and the observed `%fs:0x28` stack-guard slot was stable but **zero**.
That confirms the test fixture's call/return path, **not** Android stack
protector initialization or entropy. See [TEST_REPORTS.md](TEST_REPORTS.md).

**Short answer:** nothing found so far makes it impossible, and the parts
that were testable from here were tested. It is a large port whose hardest
part is not the loader but a bionic-to-console libc ABI layer that Cordial
never had to write, because on Linux bionic and glibc already agree on most
numbers. Six questions can only be answered on a console, and one is not a
technical question at all: **Roblox does not permit third-party clients.**

Every claim below carries its evidence. "Measured here" means it was run in
this repository's environment and the command is given; anything that is
reasoned rather than measured says **INFERRED**.

---

## 1. CPU architecture

**1.1 What CPU does the PS5 use?** An 8-core AMD Zen 2 with SMT, x86-64.
PS5_VulkanTemplate `skills/ps5-porting/references/memory-and-jit.md`
("The console's CPU is an 8-core Zen 2 with SMT"); its toolchain notes build
FFmpeg with `--cpu=znver2` (`skills/ps5-homebrew/references/toolchain.md`).
Titles are x86-64 ELF executables built for `x86_64-sie-ps5`.

**1.2 What ABI does Roblox's Android package provide?** Cordial pulled Roblox
for Android 2.732.1043 from Google Play through Waydroid: `base.apk` has no
`lib/`, and `split_config.x86_64.apk` carries `lib/x86_64/libroblox.so`,
116 MB, `ELF 64-bit LSB shared object, x86-64, for Android 26, built by NDK
r28c` (Cordial `docs/findings.md` §1.0). Cordial 0.27.0 (2026-10-08) still
loads the engine natively on x86-64 Linux (`docs/status.md`: "Loading
`libroblox.so` natively: Works").

This is a **version-specific dependency**. Cordial records that "a Roblox
version released only for ARM64 is not shown on x86_64 machines"
(`docs/status.md`, `docs/multiarch.md`): some releases ship without an x86-64
split. The installer must check every build it is given
(`akeno_package_check`, below), and the project depends on Roblox continuing
to publish x86-64 Android builds.

**1.3 Can that x86-64 code run directly on the PS5 CPU?** Yes in principle:
same instruction set, executed natively, no translator — exactly as Cordial
does on Linux, where `libbadcpu` exists only to emulate x86-64-v2/BMI
instructions on *older* CPUs (`docs/findings.md` §1.1(a)). What is not yet
shown is that the console lets a title execute code it copied into memory;
§2.4 covers how, and the self-test (§6) measures it.

**1.4 Instruction-set requirements.** The engine's floor is x86-64-v2 (SSE4.1,
SSE4.2, POPCNT; Cordial `docs/multiarch.md`). Zen 2 implements x86-64-v3
(AVX2, BMI1/2, FMA, MOVBE, LZCNT). Whether the console's kernel enables AVX
state for titles (XCR0 bits 1-2) is **not documented anywhere read**;
`akeno_cpu_query` reads CPUID and XGETBV on the console when Diagnostics runs
its probes. Measured here on the host: x86-64-v4, AVX state saved.

**1.5 Calling conventions and ABI differences.** Both sides use the System V
AMD64 calling convention. The differences that matter are in data types, not
calls — measured here with clang 18:

| Type | Android x86-64 (bionic) | PS5 (`x86_64-sie-ps5`) | Consequence |
|---|---|---|---|
| `long double` | binary128, 113-bit mantissa | x87, 64-bit mantissa | `libroblox.so` imports `powl` and `fmal`; they cannot be forwarded. They need binary128 implementations (compiler-rt's soft-float `tf` routines) |
| `pthread_mutex_t` | 40 bytes | 8 (a pointer) | fits; static initialisers differ |
| `pthread_cond_t` | 48 bytes | 8 | fits |
| `pthread_rwlock_t` | 56 bytes | 8 | fits |
| `pthread_attr_t` | 56 bytes, fields read directly | 8 | must be translated, not forwarded |
| `pthread_once_t` | **4 bytes** | **16 bytes** | forwarding `pthread_once` writes 12 bytes past the engine's object: must be shimmed |
| `sem_t` | 16 bytes | 16 bytes | same size, different layout |

bionic layouts: `libc/include/bits/pthread_types.h` and `semaphore.h` at the
bionic revision Cordial pins (`081b55b`). PS5 sizes: `sizeof` compiled with
`prospero-clang`. Command in [TEST_REPORTS.md](TEST_REPORTS.md).

**1.6 CPU feature checks.** The engine's own dispatch reads CPUID (INFERRED
from its v2 baseline; not observed). On a Zen 2 it should select AVX2 paths
if the OS saves YMM state, which is why §1.4's probe matters.

## 2. Operating system

**2.1 Which POSIX functions does the console provide?** A FreeBSD-derived
libc (`libSceLibcInternal`, `libkernel`). The payload SDK fork's platform
layer lists what a title lacks, is refused, or faults in: `access` (refused
for every path), `getcwd`, `statvfs`, the directory and `*at` families,
`getpwuid_r`, `clock_nanosleep`, `getaddrinfo`, `regcomp`, `backtrace`,
`dladdr`, `arc4random`, the `*_l` locale family, and a `localeconv` that
reports an empty decimal point (PS5_VulkanTemplate
`skills/ps5-homebrew/references/runtime-surface.md` and
`platform-layer.md`; PS5_PayloadSDK `platform/include/ps5platform/libc.h`).
PS5_Vulkan's link recipe binds each to the platform layer
(`tools/radv-link.sh`), and this repository's title uses that recipe.

**2.2 Which Linux functions does the engine need that the console lacks?**
From Cordial's import list (`docs/analysis/undefined-symbols.tsv`, 588
imports of `libroblox.so` across 10 libraries: `libandroid libc libdl libEGL
libGLESv2 liblog libmediandk libm libOpenMAXAL libOpenSLES`):

| Import | On the console | Plan |
|---|---|---|
| `epoll_create/create1/ctl/wait` | absent | emulate over `kqueue`/`kevent`, which the SDK's stubs export (measured here; that a title may call them is unproven) |
| `eventfd`, `timerfd_create/settime` | absent | a pipe; a kqueue `EVFILT_TIMER` |
| `recvmmsg`, `sendmmsg` | not exported (measured here) | loops over `recvmsg`/`sendmsg` |
| `pthread_getattr_np` | `pthread_attr_get_np` is exported | translate |
| `sched_getcpu` | `sceKernelGetCurrentCpu` | forward |
| `prctl` | absent | the subset used (thread names) |
| `getauxval` | absent | synthesise `AT_HWCAP`/`AT_PAGESZ` |
| `syscall` | titles may not make raw system calls (SIGSYS) | translate by number; **which numbers the engine uses is unknown** |
| `__system_property_get` | Android only | answer from a table, as Cordial does |
| `powl`, `fmal` | x87 `long double` | binary128 implementations (§1.5) |

**2.3 Linux system calls.** A title gets SIGSYS for system calls the console
refuses (runtime-surface.md, "Recognising one on the console"). The engine
imports `syscall` directly; Cordial forwards it to glibc, where Linux numbers
are native. Which numbers Roblox passes has not been recorded by Cordial for
x86-64 (its arm64 guest translates "arch-neutral calls" and returns `ENOSYS`
for the rest: `crates/cordial-runtime/src/guest_libc.rs`). **UNCERTAIN** until
measured with a logging wrapper on Linux.

**2.4 Memory mapping and executable memory.** Measured on the console by
PS5_PayloadSDK (`platform/docs/PROBE.md`, quoted in `ps5platform/exec.h` and
PS5_VulkanTemplate `memory-and-jit.md`):

- direct memory mapped read-write and later given execute with
  `sceKernelMprotect` runs; **execute requested at map time is refused**;
- pages are 16 KiB; a protection change costs about 26 µs;
- a mapping with no address lands in the Vulkan driver's GPU window;
- a title's flexible memory is 448 MiB by default and at most 1 GiB; direct
  memory is about 12 GiB and is where large allocations belong;
- `dlopen` and `sceKernelLoadStartModule` of a title's own unsigned `.so` are
  refused (`loading-code.md`). prospero-win loads its own modules only after
  re-linking and signing them as PRX (`docs/WINE_PS5_BUILD.md`, "PRX
  modules"), which cannot be done to Roblox's binary without modifying it.

So the engine must be loaded by an in-process loader that copies segments
into direct memory, relocates, then toggles pages to read-execute. That is
the shape of AOSP bionic's Apple-arm64 path, which already maps one RWX
`MAP_JIT` region and copies segments into it on 16 KiB pages
(`mcpelauncher-linker/bionic/linker/linker_phdr.cpp`, `ReserveAligned` and
`LoadSegments`), and it is what `akeno_elf` implements today (§6).

**2.5 Thread-local storage and signals.** A title's own TLS is emulated
(`-femulated-tls`; toolchain.md). An NDK library built for API 26 emulates its
TLS too: measured here, `tests/fixtures/fixture_tls.c` built for
`x86_64-linux-android26` has no `PT_TLS` and imports `__emutls_get_address`;
`libroblox.so` imports neither `__emutls_*` nor anything TLS-shaped
(Cordial's list), so its emulation is linked in. Two TLS-adjacent risks
remain:

- **The stack-protector canary.** Android x86-64 code reads its canary at
  `%fs:0x28` (bionic `TLS_SLOT_STACK_GUARD`). glibc keeps its own guard at
  the same offset, which is why this works for Cordial. On the console `%fs`
  is the libc's TLS base (prospero-win patch 0500 keeps it so); what lies at
  `+0x28` is undocumented. If it changes during a call, every protected
  engine function aborts. **PARTIALLY VERIFIED on 13.09:** the slot stayed stable but read as zero
  on both main and new threads. The fixture passed, but a properly seeded
  Android-style stack canary is not established.
- **Signal contexts.** The console's `ucontext_t` puts `uc_mcontext` 48 bytes
  later than FreeBSD's header, with the FP state at +320 (PS5_PayloadSDK
  `context.h`; prospero-win patches 0500/0501), and signal numbers and
  `sigaction` layouts differ between bionic (Linux numbering) and FreeBSD
  (`SIGBUS` 7 vs 10, `SIGUSR1` 10 vs 30, `SIGSYS` 31 vs 12). The engine
  imports `sigaction`, `sigaltstack`, `signal`, `raise`: all must be
  translated.

**2.6 Synchronisation.** bionic objects are larger than the console's
(§1.5) except `pthread_once_t`; a shim that keeps a pointer to a native object
inside the bionic storage (created on first use, so zero-filled static
initialisers work) is the plan. Cordial already met the same class of bug
against glibc: `pthread_cond_t` and `sem_t` overruns and a `sysconf` selector
mismatch that failed far from their cause (`docs/findings.md` §8).

**2.7 Files and network.** `/app0` is the read-only title folder; a title sees
`/download0`, and `/data` once elevated or when the loader shows it to every
title (platform contracts; prospero-win "Data directory"). `open()` fails
after about 249 files (`ps5platform/fdv.h`). Writes are throttled to about
2 MiB/s after a 1.3 GiB burst, which matters for Roblox's asset cache.
Networking: a TCP server inside a title and HTTPS out through `sceHttp2` are
proven by PS5_RetroArch; "Anything else (UDP, discovery, other services) is
unproven until a run shows it works" (platform-contracts.md, "Networking").
Roblox brings its own cURL and TLS over BSD sockets, and its game transport
is UDP (Cordial `docs/architecture.md`: "`RtcIoRna`, the real-time game
transport, is not HTTP at all"). **UDP from a title is UNCERTAIN**, and it
gates multiplayer.

## 3. Android runtime

**3.1 Which Cordial modules are independent of Linux?** Read from
`Cargo.toml` and the crate manifests:

| Part | Language | Linux-specific dependencies | Portable? |
|---|---|---|---|
| `cordial-linker-sys` + `third_party/mcpelauncher-linker` | C++ | none essential; has `__APPLE__` and `__FreeBSD__` branches in `include/compat.h` | yes, with the changes in §3.4 |
| `third_party/libjnivm` | C++ (MIT) | none known | INFERRED portable; not yet cross-built |
| `native/` framework (`game_activity.cpp`, `android_classes.cpp`, `jni_shim.cpp`, `liblog.cpp`, `opensles.cpp`) | C++ | audio backends `pipewire_backend.cpp`, `pulse_backend.cpp`, `alsa_backend.cpp`, `oss_backend.cpp` | the Android-facing halves are; the backends are replaced by AudioOut |
| `cordial-runtime` | Rust | `gtk4`, `libadwaita`, `zbus`, Wayland (`android/wayland.rs`, 7,142 lines), PipeWire | the Android framework modules (`android/asset.rs`, `looper.rs`, `input.rs`, `vulkan.rs`, `vulkan_etc.rs`, `bionic/`) are portable in substance; the crate as a whole is not |
| `cordial-shell` | Rust, GTK4/libadwaita | entirely | no: replaced by this repository's launcher |

**3.2 Which use glibc?** `cordial-run --host-libc` forwards about 502 of the
engine's imports straight to glibc (Cordial `docs/architecture.md`: "≈99
cordial · ≈502 host glibc · 49 honest stubs"). On the console each of those
must be checked against bionic's ABI rather than forwarded blindly; §1.5 and
§2.2 list the divergences found so far. This is the largest single work item.

**3.3 Which JNI operations does Roblox use?** 693 `Java_*` exports (518 in
`libroblox.so`), `JNI_OnLoad`, AGDK `GameActivity`
(`Java_com_google_androidgamesdk_GameActivity_initializeNativeCode`), and
`com.roblox.client.flags.FlagJniInterface` (Cordial `docs/findings.md` §5;
`docs/framework-api-inventory.md`). `JNI_OnLoad` cannot be called in
isolation; the Android bring-up order must be reproduced (findings §8.2), and
Cordial's `load.rs` is the working reference for that order.

**3.4 Can the AOSP linker be ported to the console?** Measured here
(`tools/diagnostics/probe-bionic-linker-ps5.sh`): building
`mcpelauncher-linker` at Cordial's pin with the console's compiler fails in
10 translation units with **18 errors, of four kinds**:

1. `stdatomic.h`'s C `_Bool` seen from C++ (bionic uses C11 `_Atomic` in C++
   headers, a Clang extension; the SDK's FreeBSD `stdatomic.h` shadows
   Clang's own);
2. `std::string dirname(const char*)` in `linker_utils.h` colliding with the
   SDK's `char *dirname(const char*)` (9 of the 18);
3. `dirname(...) + "/.version"` in `linker_config.cpp` (the same collision);
4. `zlib.h` missing (the APK zip reader).

None is architectural. The architectural change is §2.4's: map nothing from
the file, copy into a region from `ps5platform/exec.h`, toggle protections in
16 KiB pages. **IMPLEMENTABLE.**

**3.5 Can Android shared objects be mapped and called from native code?** On
a host: yes, measured here. `akeno_elf` loads real lld-linked
`x86_64-linux-android26` objects (plain RELA, APS2, RELR, Android-tagged
RELR, SysV and GNU hashes, 4 KiB and 16 KiB alignment), resolves imports from
a host table, runs constructors, calls in, and is checked by the kernel's
own `/proc/self/maps`. On the console: the same code was exercised on 13.09 with the embedded
synthetic fixtures: **HARDWARE VERIFIED for this subset** (13/13 steps,
four runs). Full AOSP linker and Roblox library remain untested.

**3.6 Can Cordial's Rust be cross-compiled for the console?** Measured here
(`tools/diagnostics/probe-rust-ps5.sh`): a Rust `std` static library built for
`x86_64-unknown-freebsd` links into a console program with only
`dl_iterate_phdr` and `pthread_setname_np` unresolved once `--gc-sections`
and `-lunwind` are used. But the library carries **native ELF TLS
relocations** (`R_X86_64_TLSGD` ×37, `TLSLD` ×16, `DTPOFF32` ×28), and the
console's TLS is emulated. A custom target with `"has-thread-local": false`
and `-Zbuild-std` (nightly) is the known route; it is not attempted yet.
Decision: the runtime's new code is C for now, and Cordial's Rust modules are
ported as needed behind C interfaces (ADR-0002).

**3.7 What replaces Android's native window?** An `ANativeWindow` handle the
runtime owns, whose Vulkan surface is created on `VK_KHR_display` (§4).

## 4. Graphics

**4.1 Does the engine have a Vulkan backend?** Yes. Cordial renders with it
(`docs/status.md`: "Rendering: Works — Vulkan"). The engine links no Vulkan:
it `dlopen`s `libvulkan.so.1`, `dlsym`s `vkGetInstanceProcAddr` (Cordial
patch 0002's trace: five `dlopen`s, seven `dlsym`s to `app ready`) and
fetches everything else through it.

**4.2 Which Vulkan surface and extensions?** It creates its surface with
`vkCreateAndroidSurfaceKHR` and needs `VK_KHR_android_surface`
(`crates/cordial-runtime/src/android/vulkan.rs`, header). It reads
`textureCompressionETC2` to decide whether to use ETC textures, and Cordial
decodes ETC2/EAC on the CPU where the driver lacks it (ADR-049). The full
feature set the engine requires has not been recorded; **UNCERTAIN** until a
trace of its `vkGetPhysicalDeviceFeatures2`/`vkCreateDevice` on Linux.

**4.3 Does it need EGL/GLES?** It imports 91 EGL/GL functions, so they must
resolve at load time, but it picks Vulkan when a loader is offered; GLES can
be honest stubs that fail (Cordial: "Selecting GLES means withholding the
virtual `libvulkan` soname"). PS5_OpenGL (OpenGL 4.6 and EGL on Gallium) is
the fallback should Vulkan fail.

**4.4 Can the console's Vulkan provide it?** RADV on the console reports
Vulkan 1.4 with `VK_KHR_display` and FIFO presentation, passed the full
1.4 CTS run with 0 failures (1,569,390 passes, 2026-09-29), and runs vkQuake,
RetroArch's Vulkan cores and Eden (PS5_Vulkan `README.md`). It does **not**
offer ETC2 or ASTC ("Not offered by RADV for GFX10.3 either": PS5_Vulkan
`docs/CTS_GAPS.md`), so Cordial's ETC2 decode is needed here too. No
`VK_KHR_android_surface`: translated to a display-plane surface, as Cordial
translates it to Wayland or Xlib. **IMPLEMENTABLE**, with §4.2's uncertainty.

**4.5 Can a frame reach VideoOut?** Yes, through RADV's `VK_KHR_display`
swapchain (PS5_VulkanTemplate: 42 of 42 programs at 3840x2160, on hardware).
The launcher, which does not link RADV yet, presents CPU-drawn frames
through VideoOut directly, the path PS5_Vulkan's demo renderer proved.

## 5. Authentication and online connectivity

**5.1 Normal sign-in without PSN?** The Android client signs in to Roblox's
own services over HTTPS; PSN is not involved anywhere in that path. Cordial
signs in with Quick Sign-in, "a code flow that needs no typing"
(`docs/status.md`).

**5.2 Embedded web components?** Some flows are web views: the captcha
(`ActivityFunCaptcha`) is on the password path, and Marketplace, Profile and
Communities are `RobloxWebActivity` pages (Cordial findings §5(d)). A title
has no embeddable web view. Quick Sign-in avoids the captcha; the web pages
would be unavailable. **UNCERTAIN** for password sign-in, **IMPLEMENTABLE**
for Quick Sign-in.

**5.3 Platform services.** None of Google Play's is needed for sign-in in
Cordial. `__system_property_get`, the Java surface `libjnivm` answers, and
Cordial's framework classes are what the engine asks for.

**5.4 Standard game servers?** Cordial joins public and private servers on
Linux. On the console this depends on UDP (§2.7). **UNCERTAIN.**

**5.5 Does Roblox permit this?** **No.** "Roblox does not support third-party
clients and bans accounts for using them, in waves, including false
positives" (Cordial `README.md`). AkenoBlox will not hide what it is, spoof
device integrity, patch the engine, or evade detection. The policy is a
standing risk to anyone who signs in, stated in the app's About screen and
the README, and the reason the project must only ever be tested with
accounts nobody cares about.

## 6. What was built and measured in this audit

| Experiment | Result | Evidence |
|---|---|---|
| ELF loader on lld-built Android x86-64 objects, 8 linkage variants | 29 test cases, 1,665 checks pass under ASan+UBSan and GCC | `tests/unit/test_elf.c`, [TEST_REPORTS.md](TEST_REPORTS.md) |
| Loader at the console's 16 KiB page size (host emulation) | W^X kept for 16 KiB-aligned objects; 4 KiB objects get shared W+X pages, counted | `test_load_console_pages`, `test_load_4k_library_on_console_pages` |
| Truncation and 2,400 random mutations | refused cleanly, never a fault | `test_truncation_sweep`, `test_mutation_sweep` |
| AOSP linker, console compiler | 10 TUs, 18 errors, 4 kinds, none architectural | `tools/diagnostics/probe-bionic-linker-ps5.sh` |
| Rust std for FreeBSD, console link | links with 2 symbols to supply; native TLS relocations present | `tools/diagnostics/probe-rust-ps5.sh` |
| The title: link, convert, sign | `eboot.bin` signed, `integrity: valid`, every import exported by a module a title loads | `tools/build/build-ps5.sh`, [TEST_REPORTS.md](TEST_REPORTS.md) |
| VideoOut tile swizzle | a bijection over each 64 KiB tile; address-order copy equals the demo's scatter | `tests/unit/test_tiling.c` |

## 7. Verdict

| Question | Verdict | Evidence | Proposed implementation | Test method | Outstanding risk |
|---|---|---|---|---|---|
| PS5 CPU is x86-64 | CONFIRMED | §1.1 | — | CPUID probe | — |
| Roblox ships x86-64 Android code | CONFIRMED (per version) | §1.2 | installer validates every build | `akeno_package_check` | ARM-only releases |
| x86-64 code runs natively | IMPLEMENTABLE | §1.3, §3.5 | copy-relocate-protect loader | Diagnostics self-test on console | refused execute |
| ISA floor (v2) met | CONFIRMED (architecture) | §1.4 | — | CPUID/XGETBV probe | AVX state not enabled |
| Calling convention | CONFIRMED | §1.5 | — | — | — |
| `long double` ABI | IMPLEMENTABLE | §1.5 measured | binary128 `powl`/`fmal` | unit tests vs reference | other `*l` imports in new builds |
| POSIX surface | IMPLEMENTABLE | §2.1 | platform layer + recipe | runtime-surface link check | refused-at-runtime calls |
| Linux-only APIs (epoll, eventfd, timerfd, mmsg) | IMPLEMENTABLE | §2.2 | emulation over kqueue/poll | host tests, console run | kqueue permission for titles |
| Raw `syscall` | UNCERTAIN | §2.3 | translate by number | log numbers under Cordial on Linux | a number with no console equivalent |
| Executable memory | IMPLEMENTABLE | §2.4 | `ps5platform/exec.h` | self-test | — |
| Emulated TLS in the engine | CONFIRMED | §2.5 | — | — | — |
| `%fs:0x28` canary | UNCERTAIN | §2.5 | none needed if stable; else a TLS layout shim | `%fs` probe + canary fixture | an unstable word, unfixable without patching the engine |
| Signals | IMPLEMENTABLE | §2.5 | translated `sigaction` | crash-handler test | engine crash handler behaviour |
| Sync primitives | IMPLEMENTABLE | §1.5, §2.6 | pointer-in-storage shim; `pthread_once` rewrite | host tests | — |
| Files | IMPLEMENTABLE | §2.7 | storage under `/data` or `/download0` | console run | write throttling, 249-file limit |
| UDP networking | UNCERTAIN | §2.7 | BSD sockets | console UDP probe | refused to titles |
| AOSP linker port | IMPLEMENTABLE | §3.4 measured | four compile fixes + copy-load path | linker loads the fixtures | — |
| libjnivm, framework layer | IMPLEMENTABLE | §3.1, §3.3 | port Cordial's `native/` and framework modules | synthetic Android app | breadth of work |
| Rust on the console | UNCERTAIN | §3.6 measured | custom target or C | probe | nightly-only route |
| Vulkan backend | CONFIRMED | §4.1 | virtual `libvulkan` over RADV | — | — |
| Engine's Vulkan features vs console RADV | UNCERTAIN | §4.2, §4.4 | ETC2 decode as Cordial | Linux trace, console run | a required feature RADV lacks here |
| Present on VideoOut | CONFIRMED | §4.5 | `VK_KHR_display` | — | — |
| Sign-in without PSN | IMPLEMENTABLE | §5.1 | Quick Sign-in | console run | — |
| Web views (captcha, pages) | UNCERTAIN | §5.2 | Quick Sign-in only | — | captcha demanded |
| Multiplayer servers | UNCERTAIN | §5.4 | — | console run | UDP |
| Roblox permits third-party clients | BLOCKED (policy) | §5.5 | none: no evasion, ever | — | account bans |

**Decision:** proceed with the Cordial-based Android x86-64 architecture
(ADR-0001). The next gate is the console self-test: if the console refuses to
execute copied code, Phase 2 stops and the blocker is recorded here.
