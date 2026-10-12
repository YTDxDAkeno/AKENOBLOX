# Testing on a console

The original build `80a3f11f9` passed its synthetic ELF loader self-test
on a real PS5 running firmware 13.09 (13/13 steps, four runs). This page
explains how to reproduce it on other builds and what still needs testing. Every result, pass or fail, goes into
[TEST_REPORTS.md](TEST_REPORTS.md) at the same level of detail.

> [!CAUTION]
> Experimental homebrew on a modified console can crash the title or the
> console. Back up what matters on it first. The tests below do not sign in
> to anything and do not touch Roblox's servers beyond one DNS lookup.

## DNS and PSN-blocking configuration

The PS5 used in the original tests had custom primary DNS `62.210.38.117`,
which the user intentionally selected to block PSN/Sony services. **Do not
change this console's DNS settings for AkenoBlox tests.** Such services and
firmware updates must remain blocked under the user's setup; the reliability
of a third-party DNS blocker is not itself guaranteed.

The updated Diagnostics screen makes two resolution checks using the existing
system resolver: `www.roblox.com` and the neutral control `example.com`.
Neither attempts an HTTPS connection or queries a Sony hostname.
- If both fail, investigate DNS server availability and the PS5 resolver.
- If the control succeeds but Roblox fails, investigate selective filtering.
- If both resolve, this proves DNS resolution only, not HTTPS, UDP or Roblox
login.
UDP loopback likewise proves local sockets only, not external multiplayer.

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
| `%fs:0x28`, main and new thread | stable and nonzero is encouraging but not full Android ABI proof | a changed or **zero** guard requires investigation; 13.09 showed stable zero |
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
