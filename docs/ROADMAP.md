# Roadmap

A milestone closes when its acceptance criteria are met, with evidence in
[TEST_REPORTS.md](TEST_REPORTS.md) — not when its files exist. "Hardware"
means a run on a real console, recorded.

| Milestone | State | Acceptance | Issue |
|---|---|---|---|
| **M0** Research and architecture | done | feasibility audit with evidence; ADRs; revisions pinned | [#9](https://github.com/YTDxDAkeno/AKENOBLOX/issues/9) |
| **M1** Native PS5 application | display/pad/storage hardware-verified; exit pending | title builds, signs and packages (done); starts from the home screen, draws, reads the pad, exits cleanly (hardware) | [#1](https://github.com/YTDxDAkeno/AKENOBLOX/issues/1) |
| **M2** ELF loading proof | **done on 13.09** (13/13 hardware probes) | loader passes host tests (done); the Diagnostics self-test passes on a console (hardware). If it fails, the blocker is recorded and Roblox work stops | [#2](https://github.com/YTDxDAkeno/AKENOBLOX/issues/2) |
| **M3** Android compatibility runtime | not started | AOSP linker ported; bionic ABI layer; libjnivm; a synthetic Android library initialised through `JNI_OnLoad` and GameActivity on a host and a console | [#3](https://github.com/YTDxDAkeno/AKENOBLOX/issues/3) |
| **M4** Graphics and audio | not started | RADV linked; a synthetic Android Vulkan program draws through the virtual `libvulkan` and `VK_KHR_display`; OpenSL ES plays through AudioOut | [#4](https://github.com/YTDxDAkeno/AKENOBLOX/issues/4) |
| **M5** Roblox engine initialisation | not started | the user's `libroblox.so` loads, constructors run, `JNI_OnLoad` and GameActivity's native init complete, the engine logs its own start-up | [#5](https://github.com/YTDxDAkeno/AKENOBLOX/issues/5) |
| **M6** Authentication and networking | not started | HTTPS from the engine; Quick Sign-in completes; UDP works from a title | [#6](https://github.com/YTDxDAkeno/AKENOBLOX/issues/6) |
| **M7** First verified experience | not started | a public experience loads and the player moves, on a console | [#7](https://github.com/YTDxDAkeno/AKENOBLOX/issues/7) |
| **M8** Alpha release | not started | M1–M7 on hardware, documented, reproducible | [#8](https://github.com/YTDxDAkeno/AKENOBLOX/issues/8) |

## Next concrete tasks

In dependency order:

1. **Record the successful M2 hardware evidence** and verify the separate
   clean exit requirement for M1. The original build `80a3f11f9` passed
   13/13 ELF loader probes on PS5 firmware 13.09 (TEST_REPORTS.md).
   `%fs:0x28` was stable but zero; investigate guard initialization.
2. **Record the engine's system calls and Vulkan features on Linux** under
   Cordial with logging wrappers (FEASIBILITY §2.3, §4.2): two of the
   UNCERTAIN rows, answerable without a console.
3. **Port the AOSP linker** (M3): the four compile fixes and the copy-load
   path over `akeno_mem_ops`, as patches against Cordial's pin; host tests
   loading the existing fixtures through it, compared with `akeno_elf`.
4. **The bionic ABI layer** (M3): errno, signals, sockets, stat, pthread
   (`pthread_once` first), epoll over kqueue, `long double`, each with a host
   test that runs the bionic-side expectation against the shim.
5. **Hardware-test the new local UDP probe** (PR #10); this tests only loopback
   socket functionality, not external Roblox UDP. Investigate the DNS error 4
   from the first 13.09 run.
6. **Build RADV** for the engine (M4): PS5_Vulkan's `tools/build-radv.sh
   release` and its host LLVM/libclc prerequisites, in CI if it fits.

## Release plan

| Version | Content | Condition |
|---|---|---|
| v0.1.0-alpha | launcher, diagnostics, ELF self-test | built now; titled as a test build until a console run |
| v0.2.0-alpha | AOSP linker and bionic layer, synthetic Android tests | M3 on hardware |
| v0.3.0-alpha | Roblox engine loading experiments | labelled experimental unless the engine starts |
| v0.4.0-alpha | graphics and runtime integration | M4–M5 |
| v0.5.0-alpha | first verified gameplay | only if M7 is demonstrated |
