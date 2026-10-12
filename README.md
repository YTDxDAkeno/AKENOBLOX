# AkenoBlox PS5

An experimental homebrew runtime that aims to run **Roblox's own Android
x86-64 engine locally on a jailbroken PS5**: native code on the console's CPU,
frames from its GPU, no PSN, no streaming, no PC once installed.
GPL-3.0-or-later.

> [!WARNING]
> **AkenoBlox does not run Roblox yet.** This is v0.1.0-alpha: a launcher, the
> first runtime layers, and a hardware self-test. **Its ELF loader self-test passed on one PS5 running firmware 13.09**;
> Roblox has NOT run. Other systems and firmwares remain unverified. What is done and what is not: [Runtime status](#status).

## Why it might work

[Cordial](https://github.com/luohoa97/cordial) runs Roblox's official Android
engine natively on x86-64 Linux, with no emulator: a ported AOSP bionic
linker maps `libroblox.so`, `libjnivm` stands in for Android's Java runtime,
and a framework layer answers the engine's calls into Android. The PS5's CPU
is x86-64 too. AkenoBlox ports that design onto the console's own platform
(PS5_VulkanTemplate, PS5_Vulkan's RADV driver, PS5_PayloadSDK's platform
layer) instead of Linux's. The evidence, and the open questions, are in
[docs/FEASIBILITY.md](docs/FEASIBILITY.md).

## Status

| Layer | State |
|---|---|
| Launcher: menu, settings, diagnostics, DualSense, VideoOut display | firmware 13.09: display/pad/storage verified; clean exit unverified |
| ELF loader (`akeno_elf`): RELA, APS2, RELR, IFUNC, constructors | 13/13 embedded PS5 hardware probes passed (four runs); extended features host-tested |
| AOSP linker port | probed: 18 compile errors, all understood |
| bionic ABI on the console's libc | not started |
| JNI VM, Android framework, graphics, audio | not started |
| Roblox engine, sign-in, gameplay | not started |

Full table: [docs/PROJECT_STATE.md](docs/PROJECT_STATE.md). Evidence:
[docs/TEST_REPORTS.md](docs/TEST_REPORTS.md).

**The most useful thing you can do** if you have a jailbroken PS5 is run the
[hardware tests](docs/HARDWARE_TESTING.md) and report what happened.

## Install

Copy the release's `PPSA99256` folder to `/data/homebrew/` and register it with
your loader (ShadowMountPlus or similar):
[docs/INSTALLATION.md](docs/INSTALLATION.md). AkenoBlox never downloads or
ships Roblox; you supply `libroblox.so` from your own copy of the Android app.

## Build

```bash
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build
tools/build/build-ps5.sh     # the console title and release ZIP
```

Details and pinned dependencies: [docs/BUILDING.md](docs/BUILDING.md).

## What AkenoBlox is not

- **Not affiliated with Roblox Corporation or Sony Interactive Entertainment**,
  and not endorsed by either. Roblox is a trademark of Roblox Corporation;
  PlayStation and PS5 are trademarks of Sony Interactive Entertainment Inc.
- **Not permitted by Roblox.** Roblox does not support third-party clients and
  bans accounts that use them. Never sign in with an account you care about.
- **Not a cheat, mod injector or bypass.** It will never patch the engine, hook
  into it, spoof device integrity or evade detection
  ([ADR-0008](docs/DECISIONS.md)).

## Documentation

[Architecture](docs/ARCHITECTURE.md) ·
[Decisions](docs/DECISIONS.md) ·
[Roadmap](docs/ROADMAP.md) ·
[Porting notes](docs/PORTING_NOTES.md) ·
[Research log](docs/RESEARCH_LOG.md) ·
[Compatibility](docs/COMPATIBILITY.md) ·
[Security](docs/SECURITY.md) ·
[For agents](AGENTS.md) ·
[Contributing](CONTRIBUTING.md)

## Licence

GPL-3.0-or-later ([LICENSE](LICENSE)). Third-party work keeps its own licence;
see [NOTICE.md](NOTICE.md).
