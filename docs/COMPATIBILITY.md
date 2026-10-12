# Compatibility

## Consoles and firmware

| Console | Firmware | Loader | State |
|---|---|---|---|
| — | — | — | **no console run recorded yet** |

AkenoBlox uses no firmware-specific offsets: it calls exported functions only,
through the payload SDK and PS5_PayloadSDK's platform layer. What depends on
firmware is the jailbreak, the enabler and the loader that registers the title.
Reports are added here from [TEST_REPORTS.md](TEST_REPORTS.md).

## Roblox builds

| Roblox Android version | x86-64 split | Loads under AkenoBlox | Notes |
|---|---|---|---|
| — | — | — | nothing loads the engine yet |

Cordial reports Roblox for Android 2.732.1043 ships
`lib/x86_64/libroblox.so` (116 MB, Android 26, NDK r28c), and that some
releases are ARM64-only. The package check reports, for any build you give it,
whether it is x86-64, its API level and NDK, and how many imports it has.

## Controllers and input

| Device | State |
|---|---|
| DualSense, player 0 | implemented; not yet run on a console |
| More players | not implemented (the launcher needs one) |
| USB keyboard and mouse | not implemented |
