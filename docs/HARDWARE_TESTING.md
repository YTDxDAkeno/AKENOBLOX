# Testing on a console

Nothing in this project has run on a PS5 yet. This page is how to change
that, and exactly what to send back. Every result, pass or fail, goes into
[TEST_REPORTS.md](TEST_REPORTS.md) at the same level of detail.

> [!CAUTION]
> Experimental homebrew on a modified console can crash the title or the
> console. Back up what matters on it first. The tests below do not sign in
> to anything and do not touch Roblox's servers beyond one DNS lookup.

## What you need

- A PS5 running a homebrew environment: a jailbreak for its firmware, an
  enabler such as etaHEN or kstuff, a loader that lists titles in
  `/data/homebrew` on the home screen (ShadowMountPlus), an FTP server on port
  2121 (ftpsrv) and klogsrv on port 3232 — the environment PS5_VulkanTemplate's
  titles are tested in.
- A PC on the same network with Python 3, and a release ZIP (or your own
  build, [BUILDING.md](BUILDING.md)).

## Test 1 — the title starts, draws and reads the pad

1. Unzip the release; copy `PPSA99256/` to `/data/homebrew/PPSA99256/`
   (`python3 tools/deploy/deploy.py title --host <console address>` does it).
2. Start klog capture on the PC:
   `python3 tools/deploy/klog.py --host <console address> --out test1.txt`
3. Make the title appear on the home screen with your loader and start it.
4. Expected: the AkenoBlox home screen at 1080p within a few seconds; the
   header says "Controller connected"; the D-pad moves the selection.
5. Choose **Exit**, then press Cross: expected to return to the PS5 home
   screen with no crash report.

Record: firmware, loader and its version, whether the screen appeared, what it
looked like (a photo helps: colours swapped or a scrambled image both mean
something specific), whether the pad worked, how it exited, and `test1.txt`.

## Test 2 — the hardware probes and the ELF self-test (Phase 2's gate)

1. Start AkenoBlox; open **Diagnostics**; press **Cross** (Run probes).
2. Expected: a notice "Probes done: loader self-test PASSED" and the rows of
   the CPU, `%fs:0x28`, ELF loader and storage sections.
3. Press **Square** to save `diagnostics.txt`. It is written to
   `/data/akenoblox/` or `/download0/akenoblox/` (the Storage row says which).
4. Fetch `diagnostics.txt` and `logs/akenoblox.log` over FTP, plus the klog
   capture.

What the results mean:

| Row | Pass means | Fail means |
|---|---|---|
| x86-64 level | v2 or better: the engine's instructions run | v1: instruction emulation needed (libbadcpu) |
| AVX state saved | the kernel enables AVX for titles | AVX code paths would fault |
| `%fs:0x28`, main and new thread | Android's stack protector can work unmodified | **a blocker for the current architecture**: record it exactly |
| ELF self-test: copy, relocate, protect | the console executes code a title copied in | **Phase 2 blocked**: record the step that failed |
| Android stack protector | a protected call completed | the canary word moved during a call |
| UDP IPv4 loopback | local datagram sockets work | inspect errno and console network initialization; external multiplayer remains untested either way |

If the title **crashes** during the probes, the last `[AkenoBlox]` line in klog
names the probe that was running (each is logged before it starts). Send the
whole capture, including the crash record.

## Test 3 — the package check (optional)

1. From your own copy of Roblox's Android app, take
   `lib/x86_64/libroblox.so` (from `split_config.x86_64.apk`; see
   [INSTALLATION.md](INSTALLATION.md)).
2. `python3 tools/deploy/deploy.py roblox --host <console address> /path/to/libroblox.so`
3. In AkenoBlox choose **Start Roblox**. Expected: "Roblox cannot start yet",
   then the package's size, Android API level, NDK version and import count.
   Nothing is executed.

## Reporting

Open a "Hardware test report" issue (the template asks for everything above).
Do not paste account names, cookies or tokens; nothing in these tests produces
any, and the log masks the usual shapes (docs/SECURITY.md).
