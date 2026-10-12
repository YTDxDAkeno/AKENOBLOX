# Changelog

## 0.1.0-alpha — unreleased

The first build: a launcher and the runtime's first layers. **It does not run
Roblox.** Built, signed and packaged as a PS5 title; not yet run on a console.

### Added

- A native PS5 title (`PPSA99256`): home, Start Roblox, Runtime Status,
  Diagnostics, Controller (with an input test), Graphics & Audio, About, Exit;
  DualSense navigation; CPU-drawn 1920x1080 frames through VideoOut; the
  shell exit; logs to klog and to storage.
- `akeno_elf`, a bounded ELF64 x86-64 loader: RELA, Android APS2 and RELR
  relocations, GNU and SysV hashes, IFUNC on request, constructors and
  destructors, a missing-import report that names every missing symbol;
  memory through the platform (`ps5platform/exec.h` on the console).
- Diagnostics: CPUID and the x86-64 level, the `%fs:0x28` canary probe, the ELF
  self-test over embedded fixtures, storage and DNS checks; a report saved to
  `diagnostics.txt`.
- A package check that validates a user-supplied `libroblox.so` without
  running it.
- DualSense-to-Android gamepad mapping (KeyEvent keycodes, MotionEvent axes),
  configurable.
- Settings with validation and atomic saves; a log that masks credentials.
- Host build with scripted input and PPM frames; 9 test suites; libFuzzer
  targets; the feasibility experiments as scripts.

### Known problems

- Nothing has run on a console. The display, pad, storage and self-test code
  follows console-proven references but is unverified here.
- Storage falls back to `/download0` when `/data` is not visible; whether a
  title may write there, and how to reach it over FTP, is unverified.
- Only player 0's controller is read; no keyboard or mouse.
