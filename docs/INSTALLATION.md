# Installing AkenoBlox on a PS5

> [!WARNING]
> **AkenoBlox does not run Roblox yet.** This release is the launcher and the
> first layers of the runtime, published so it can be tested on hardware
> ([HARDWARE_TESTING.md](HARDWARE_TESTING.md)). It is experimental and has not
> been run on a console by the project.

## Requirements

- A jailbroken PS5 with a homebrew environment: an enabler (etaHEN, kstuff),
  a loader that shows titles from `/data/homebrew` on the home screen
  (ShadowMountPlus), and an FTP server (ftpsrv, port 2121). Firmware support
  is your jailbreak's and loader's; AkenoBlox itself uses no firmware
  offsets.
- No PSN account. No PC is needed once it is installed.

## Install

1. Download `AkenoBlox-PS5-<version>.zip` from the Releases page and unzip it.
2. Copy the `PPSA99256` folder to `/data/homebrew/PPSA99256` on the console,
   with any FTP client, or:
   `python3 tools/deploy/deploy.py title --host <console address> PPSA99256`
3. Let your loader register it; AkenoBlox appears on the home screen.

## Where it keeps its files

`/data/akenoblox/` when `/data` is visible to titles, otherwise
`/download0/akenoblox/`: `settings.ini`, `logs/akenoblox.log` (the previous
run's is `.1`) and `diagnostics.txt` when you save one. Diagnostics shows
which.

## Your Roblox files

AkenoBlox never downloads Roblox and never ships any of it. When the runtime
can use it, you supply the engine from your own copy of the Android app:

1. Get Roblox's Android app as split APKs (for example from your own Android
   device with `adb shell pm path com.roblox.client`, then `adb pull` each
   path). The engine is in `split_config.x86_64.apk`, not `base.apk`.
2. Unzip `split_config.x86_64.apk` (it is a zip) and take
   `lib/x86_64/libroblox.so`. An `arm64-v8a` library will not work: the PS5's
   CPU is x86-64.
3. Copy it to `<storage>/roblox/lib/x86_64/libroblox.so`, or run
   `python3 tools/deploy/deploy.py roblox --host <console address> libroblox.so`.
4. **Start Roblox** checks it and reports what it found. It does not run it.

## Uninstall

Remove the title from your loader, then delete `/data/homebrew/PPSA99256/` and
`/data/akenoblox/` (or `/download0/akenoblox/`). AkenoBlox writes nowhere else.
