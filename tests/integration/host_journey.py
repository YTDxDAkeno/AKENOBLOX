#!/usr/bin/env python3
# AkenoBlox PS5 - the host program, driven through every screen.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   host_journey.py <akenoblox-host> <work-dir>
#
# Runs the real program with scripted input: Start Roblox, Runtime Status,
# Diagnostics (runs the probes and saves the report), Controller and its
# input test, Graphics & Audio (changes the resolution), About, then Exit.
# Checks what it leaves behind: the settings it saved, the report, the log and
# the last frame. Then runs --diagnostics on its own.
import pathlib
import shutil
import subprocess
import sys

JOURNEY = ",".join([
    "cross", "wait:2", "circle",                                   # Start Roblox, back
    "down", "cross", "down", "down", "circle",                     # Runtime Status, scroll, back
    "down", "cross", "cross", "wait:3", "square", "wait:2", "r1",  # Diagnostics: probes, save, page
    "circle",
    "down", "cross", "down", "down", "down", "down", "cross",       # Controller, Input test
    "wait:3", "hold:circle:70", "wait:2", "circle",                 # hold Circle out, back home
    "down", "cross", "right", "circle",                            # Graphics: 2560x1440, save
    "down", "cross", "circle",                                     # About
    "down", "cross", "cross",                                      # Exit, confirm
])


def fail(message: str) -> None:
    print(f"host_journey: FAIL: {message}")
    sys.exit(1)


def main() -> int:
    binary, work = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    storage = work / "storage"
    shot = work / "last-frame.ppm"
    run = subprocess.run(
        [str(binary), "--fast", "--frames", "3000", "--storage", str(storage),
         "--screenshot", str(shot), "--script", JOURNEY],
        capture_output=True, text=True, timeout=300)
    (work / "journey.log").write_text(run.stderr)
    if run.returncode != 0:
        print(run.stderr[-3000:])
        fail(f"exit status {run.returncode}")

    settings = (storage / "settings.ini").read_text()
    if "resolution=2560x1440" not in settings:
        fail("the resolution change was not saved:\n" + settings)
    report = (storage / "diagnostics.txt").read_text()
    if "Self-test" not in report or "PASSED" not in report:
        fail("the saved report does not show a passed self-test:\n" + report)
    log = (storage / "logs" / "akenoblox.log").read_text()
    for expected in ("selftest: PASSED", "probes: CPUID", "leaving after"):
        if expected not in log:
            fail(f"the log lacks {expected!r}")
    header = shot.read_bytes()[:20]
    if not header.startswith(b"P6\n1920 1080\n255\n"):
        fail(f"the screenshot header is {header!r}")
    if len(shot.read_bytes()) != len(b"P6\n1920 1080\n255\n") + 1920 * 1080 * 3:
        fail("the screenshot is the wrong size")

    # --diagnostics: probes, report, exit status from the self-test.
    alone = work / "diagnostics-only.txt"
    run = subprocess.run([str(binary), "--storage", str(work / "storage2"), "--diagnostics", str(alone)],
                         capture_output=True, text=True, timeout=120)
    if run.returncode != 0:
        print(run.stderr[-3000:])
        fail(f"--diagnostics exited {run.returncode}")
    text = alone.read_text()
    for expected in ("[ELF loader]", "[CPU]", "x86-64 level", "PASSED"):
        if expected not in text:
            fail(f"--diagnostics report lacks {expected!r}")
    print("host_journey: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
