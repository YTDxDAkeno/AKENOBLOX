# Porting notes: bionic on the console

What changes when Android code built for bionic calls into the console's
FreeBSD-derived libc. Cordial forwards about 502 of the engine's imports to
glibc unchanged, which works because glibc and bionic share Linux's numbers.
On the console, every forwarded import is a question. This page is the
running answer; each row says how it was established.

## Measured differences

| Area | bionic (Android x86-64) | Console (`x86_64-sie-ps5`) | How known | Plan |
|---|---|---|---|---|
| `long double` | binary128 (113-bit mantissa) | x87 (64-bit mantissa) | `__LDBL_MANT_DIG__` with clang 18 for both targets | `powl`, `fmal` (imported by the engine) get binary128 implementations |
| `pthread_once_t` | 4 bytes | 16 bytes | bionic `pthread_types.h`; `sizeof` with `prospero-clang` | rewrite `pthread_once` over a native once kept elsewhere |
| `pthread_mutex_t` / `cond_t` / `rwlock_t` | 40 / 48 / 56 bytes | 8 / 8 / 8 (pointers) | same | the native object's pointer inside bionic's storage, made on first use |
| `pthread_attr_t` | 56-byte struct the engine may read | 8 (a pointer) | same | translate each `pthread_attr_*` |
| `sem_t` | 16 bytes, `count` first | 16 bytes, FreeBSD layout | same | shim |
| Page size | 4 KiB (Android x86-64) | 16 KiB | PROBE.md via `memory-and-jit.md` | loader works in 16 KiB pages; 4 KiB-aligned objects get shared W+X pages (`akeno_elf` counts them) |
| Executable memory | `mmap(PROT_EXEC)` | refused at map time; `sceKernelMprotect` after | PROBE.md | copy-load, then toggle (`ps5platform/exec.h`) |
| `epoll_*`, `eventfd`, `timerfd_*` | Linux | absent; `kqueue`/`kevent` exported by the stubs | `llvm-nm` over the SDK's stubs | emulate |
| `recvmmsg`, `sendmmsg` | Linux | not exported | same | loops |

## Known from reading, not yet measured

| Area | Difference | Source |
|---|---|---|
| `errno` values | Linux numbering vs FreeBSD (`EAGAIN` 11 vs 35, `EINPROGRESS` 115 vs 36, ...) | both systems' headers |
| Signal numbers | `SIGBUS` 7/10, `SIGUSR1` 10/30, `SIGCHLD` 17/20, `SIGSYS` 31/12 | both systems' headers |
| `ucontext_t` | console context 48 bytes later than FreeBSD's header; FP state at +320 | PS5_PayloadSDK `context.h`; prospero-win patches 0500/0501 |
| `struct stat`, `dirent` | different layouts | both systems' headers |
| `open` flags, `mmap` flags, `fcntl` commands | different values (`O_CREAT` 0100 vs 0x200, `MAP_ANONYMOUS` 0x20 vs 0x1000) | both systems' headers |
| Sockets | `SOL_SOCKET` 1 vs 0xffff; `AF_INET6` 10 vs 28; BSD `sockaddr` has a length byte; `addrinfo` orders `ai_canonname` and `ai_addr` differently | both systems' headers |
| `sysconf` selectors | 145 of 147 differ from glibc's (Cordial generated a table); FreeBSD's differ again | Cordial `docs/findings.md` §8 |
| `clock_gettime` clock ids | `CLOCK_MONOTONIC` 1 vs 4; `CLOCK_BOOTTIME` Linux-only | both systems' headers |
| `%fs:0x28` | bionic's stack-protector canary; on the console, undocumented | Diagnostics probe pending |

## The AOSP linker port

Four compile fixes (FEASIBILITY §3.4) and one behavioural change:

1. Compile its C++ with Clang's own `stdatomic.h` ahead of the SDK's.
2. Rename the linker's `std::string dirname(const char*)` (it collides with
   the SDK's `char *dirname(const char*)`), and its one use in
   `linker_config.cpp`.
3. Build zlib for the console (PS5_Vulkan builds the same pinned 1.3.2 for
   the host) or drop the APK zip reader, which Cordial does not use for the
   engine (it extracts `lib/` once).
4. Replace `ReserveAligned`/`LoadSegments`/the protect functions in
   `linker_phdr.cpp` with a path over `akeno_mem_ops`: reserve in direct
   memory, copy each segment, zero its tail, relocate, toggle protections.
   bionic's Apple-arm64 branch is the precedent: one RWX `MAP_JIT` region,
   copied into, on 16 KiB pages.

Patches go in `patches/` against the revision Cordial pins, applied by the
build into a copy, as Cordial does (`patches/apply.rs` there).

## Memory budget

Cordial budgets about 1.5 GB per running engine (its ADR-012). On the console
that has to come from direct memory: the engine's `malloc` must reach the
platform heap (`ps5platform/heap.h`), not libc's private heap, which "ran out
long before the title in every project that relied on it". The engine's 116
MB of code goes into an `exec.h` region, also direct memory.

## Writing to storage

Roblox keeps an asset cache (`RbxStorage`, a SQLite database, Cordial
`docs/status.md`). After a 1.3 GiB burst, a title's writes drop to about
2 MiB/s (`platform-layer.md`); first launches that fill the cache will meet
that. `ps5platform/offload.h` writes through the console's FTP server at about
7–22 MiB/s instead, and is the first thing to try if it bites.
