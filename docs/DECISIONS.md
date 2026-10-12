# Decisions

Each decision says what was decided, what else was considered, and why, so
that somebody about to contradict one can find the reasoning. Arguing with a
decision is welcome; quietly contradicting one in code is not. When a
decision changes, mark the old one superseded here rather than deleting it.

---

## ADR-0001: Cordial's Android x86-64 runtime, ported, is the architecture

**Status:** accepted, 2026-10-12.

**Decision.** Run Roblox's official Android x86-64 engine in-process inside a
native PS5 title, through a port of Cordial's runtime pieces (AOSP bionic
linker, libjnivm, the Android framework layer) onto a new PS5 platform layer.
Cordial's Linux desktop half (GTK4/libadwaita shell, Wayland, PipeWire,
D-Bus) is not ported; the launcher is this repository's own.

**Considered.**
- *Porting Cordial whole.* Its runtime crate depends on `gtk4`, `libadwaita`,
  `zbus` and Wayland directly (`crates/cordial-runtime/Cargo.toml`), and its
  window is a Wayland subsurface of the shell's (Cordial ADR-011). None of
  that exists on the console.
- *Windows Roblox through prospero-win's Wine.* The Windows client's
  anti-tamper (Hyperion) has historically refused Wine, and working around it
  is out of scope by rule. Not pursued unless Roblox changes that.
- *The official PS5 Roblox application.* Requires PSN and Sony's licensing;
  out of scope by rule.

**Why.** Cordial demonstrates real gameplay from exactly this engine on x86-64
(`docs/status.md`), and the console runs x86-64 code natively. The work is a
port of a known-working design, not new research into whether the engine can
run outside Android. [FEASIBILITY.md](FEASIBILITY.md) has the evidence and the
open questions.

---

## ADR-0002: New runtime code is C11; Rust waits for a console target

**Status:** accepted, 2026-10-12.

**Decision.** The launcher, the platform layer and the loader are C11, built
with the payload SDK's clang. Cordial's Rust modules are ported as needed —
re-expressed in C behind the same behaviour, or kept in Rust once a console
Rust target exists — with each port naming the Cordial file and revision it
follows.

**Considered.** Building Cordial's Rust for `x86_64-unknown-freebsd` and
linking it in. Measured (FEASIBILITY §3.6): it links, but `std` carries native
ELF TLS relocations and the console's TLS is emulated; the fix is a custom
target with `has-thread-local: false` and `-Zbuild-std` on nightly. That is
worth revisiting, not worth blocking on.

**Why.** Everything proven on the console so far (RADV, the platform layer,
every reference title) is C and C++ through this compiler.

---

## ADR-0003: A small loader of our own now, the AOSP linker for the engine

**Status:** accepted, 2026-10-12.

**Decision.** `akeno_elf` is a bounded ELF64 x86-64 loader (copy, relocate
RELA/APS2/RELR, protect, IFUNC on request, constructors, symbol lookup). It is
(1) the Phase 2 hardware proof that the console executes copied code,
(2) the validator the installer runs over a user-supplied `libroblox.so`,
and (3) the reference the AOSP linker port is checked against. The engine
itself will be loaded by Cordial's AOSP bionic linker, ported.

**Considered.** Only porting the AOSP linker first. Its first console run would
then test four things at once (the port's compile fixes, its new copy-load
path, executable memory, and the libc shims it calls), so a failure would not
say which. `akeno_elf` tests only the platform's executable memory and the
relocation formats, and is small enough to fuzz.

**Why not use `akeno_elf` for the engine?** Cordial's linker has been run
against the real `libroblox.so` (relocations, constructors, symbol
versioning, `dl_iterate_phdr`, the `dlopen` the engine does at run time);
`akeno_elf` has not, and duplicating that work would be a rewrite without
evidence it is needed.

---

## ADR-0004: The launcher and the engine are two processes of one title

**Status:** accepted, 2026-10-12.

**Decision.** The launcher (this program) owns VideoOut while it runs. Starting
Roblox will hand over with `sceSystemServiceLoadExec` to a second executable
of the same title, in a fresh process, which owns the display through RADV's
`VK_KHR_display` swapchain; leaving the game hands back the same way.

**Considered.** One process that closes its VideoOut and opens RADV. RADV's
WSI owns the console's one VideoOut through its swapchain and must not share
it (PS5_VulkanTemplate `vulkan-on-radv.md`), and a crash in the engine would
take the launcher with it.

**Why.** It is the pattern proven on the console: PS5_RetroArch hands over
between its picker, EmulationStation and RetroArch this way, 0.56–0.61 s per
switch with flexible memory back to a cold launch's (platform-contracts.md,
"Restarting as another program of the title"), and prospero-win runs one game
per process the same way. Cordial reached the same split for its own reasons
(Cordial ADR-055).

---

## ADR-0005: The launcher draws on the CPU and presents through VideoOut

**Status:** accepted, 2026-10-12.

**Decision.** The launcher renders its UI in software into a 1920x1080 frame,
which the console platform tiles into VideoOut buffers using exactly the
allocation, pixel format, tiling and flip PS5_Vulkan's demo renderer used on
hardware.

**Considered.** Building the launcher on PS5_VulkanTemplate's Vulkan base
class and UI kit. That needs the RADV archive, which needs PS5_Mesa built with
host LLVM, libclc and the SPIR-V translator (PS5_Vulkan `tools/build-radv.sh`):
a long build not yet set up here.

**Why.** It gets a runnable title now with no GPU driver in it, makes every
screen testable on a host by its pixels, and keeps the launcher independent of
the driver the engine will stress. The engine's own rendering is Vulkan on
RADV regardless (ADR-0004). Revisit when RADV is in the build for the engine:
the launcher can move to Vulkan then if it gains something.

---

## ADR-0006: GPL-3.0-or-later

**Status:** accepted, 2026-10-12.

**Decision.** The project is GPL-3.0-or-later.

**Why.** It will incorporate Cordial (GPL-3.0-or-later), and the title already
links PS5_PayloadSDK's platform layer (GPL-3.0-or-later) and PS5_Vulkan's CRT
(GPL-3.0-or-later). MIT pieces (PS5_VulkanTemplate) and public-domain ones
(font8x8) are compatible. prospero-win (LGPL-2.1-or-later) and Playden
(GPL-3.0) are read for their designs; no code from either is copied. A source
that carries no licence (minecraft-linux's `libc-shim`, whose GitHub licence
field is empty) is not copied from at all. [NOTICE.md](../NOTICE.md) lists
every incorporated work.

---

## ADR-0007: Roblox's files are the user's; the project ships none

**Status:** accepted, 2026-10-12.

**Decision.** No Roblox binary, asset or APK is committed, downloaded by the
program, or packaged. The user copies `lib/x86_64/libroblox.so` (and later the
APK's assets) from their own copy of the Android app; the installer validates
it and never modifies it. The release packager refuses to build a ZIP with a
`*roblox*.so` or an APK in it.

**Why.** Redistribution rights do not exist. Cordial's rule is the same ("No
Roblox code, ever"), and it is a condition of this project's legality.

---

## ADR-0008: No evasion, no patching, no hiding

**Status:** accepted, 2026-10-12.

**Decision.** AkenoBlox never patches the engine, hooks into it, spoofs device
integrity, forges tokens, rewrites its network traffic, or hides that it is a
third-party runtime. Where Roblox refuses the runtime, that is recorded as a
blocker (FEASIBILITY §5.5), not worked around. The loader keeps the engine's
text read-execute, and IFUNC resolution is off unless a caller asks.

**Why.** The project brief requires it, Cordial holds the same line (its
ADR-001), and anything else would put users' accounts and the project at
greater risk than they already are.

---

## ADR-0009: Only proven console calls, and a self-test before claims

**Status:** accepted, 2026-10-12.

**Decision.** The console platform calls only exported functions, in the shapes
a console run has used (each file names its source), and a hardware claim is
made only from a console run's log. Probes that execute unusual instructions
or foreign code (CPUID, `%fs:0x28`, loaded code) run only when the user asks,
each logged before it runs.

**Why.** The commonest console failure is a call that links and does not exist
at run time (runtime-surface.md); the second is a crash nobody can attribute.
Logging before each probe makes a crash attributable from klog alone.
