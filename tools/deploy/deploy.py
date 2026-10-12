#!/usr/bin/env python3
# AkenoBlox PS5 - copy the title (and, separately, your own libroblox.so) to a
# console over FTP.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   deploy.py title [--host H] [--port 2121] [dist/PPSA99256]
#   deploy.py roblox [--host H] [--port 2121] /path/to/your/libroblox.so
#
# The host comes from --host or PS5_HOST. Needs an FTP server on the console
# (ftpsrv on 2121 is what PS5_VulkanTemplate documents). The console's server
# deviates from the standard in three ways this handles
# (PS5_VulkanTemplate title-packaging.md, "Deploying"): it answers a
# successful DELE with 226, resolves paths from the root, and lists
# inconsistently with a path argument. It also keeps its own transformed form
# of an accepted eboot.bin, so eboot.bin is checked by upload, not read back.
#
# NOT YET RUN AGAINST A CONSOLE. Report what it does (docs/HARDWARE_TESTING.md).
import argparse
import ftplib
import os
import pathlib
import sys

TITLE_REMOTE = "/data/homebrew/PPSA99256"
ROBLOX_REMOTE = "/data/akenoblox/roblox/lib/x86_64"


def connect(host: str, port: int) -> ftplib.FTP:
    ftp = ftplib.FTP()
    ftp.connect(host, port, timeout=30)
    ftp.login()
    return ftp


def makedirs(ftp: ftplib.FTP, path: str) -> None:
    built = ""
    for part in path.strip("/").split("/"):
        built += "/" + part
        try:
            ftp.mkd(built)
        except ftplib.error_perm:
            pass  # exists


def upload(ftp: ftplib.FTP, local: pathlib.Path, remote: str) -> None:
    with local.open("rb") as handle:
        ftp.storbinary(f"STOR {remote}", handle)
    if local.name == "eboot.bin":
        return
    try:
        size = ftp.size(remote)
    except ftplib.all_errors:
        size = None
    if size is not None and size != local.stat().st_size:
        raise SystemExit(f"{remote}: {size} bytes on the console, {local.stat().st_size} sent")


def deploy_title(ftp: ftplib.FTP, folder: pathlib.Path) -> None:
    if not (folder / "eboot.bin").is_file() or not (folder / "sce_sys" / "param.json").is_file():
        raise SystemExit(f"{folder} is not a built title folder (run tools/build/build-ps5.sh)")
    for local in sorted(folder.rglob("*")):
        if local.is_dir():
            continue
        relative = local.relative_to(folder).as_posix()
        remote = f"{TITLE_REMOTE}/{relative}"
        makedirs(ftp, os.path.dirname(remote))
        upload(ftp, local, remote)
        print(f"  {relative} ({local.stat().st_size} bytes)")


def deploy_roblox(ftp: ftplib.FTP, library: pathlib.Path) -> None:
    head = library.read_bytes()[:20]
    if head[:4] != b"\x7fELF" or head[4] != 2 or int.from_bytes(head[18:20], "little") != 62:
        raise SystemExit(f"{library} is not an x86-64 ELF: copy lib/x86_64/libroblox.so, not arm64-v8a")
    makedirs(ftp, ROBLOX_REMOTE)
    upload(ftp, library, f"{ROBLOX_REMOTE}/libroblox.so")
    print(f"  libroblox.so ({library.stat().st_size} bytes) -> {ROBLOX_REMOTE}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("what", choices=["title", "roblox"])
    parser.add_argument("path", nargs="?")
    parser.add_argument("--host", default=os.environ.get("PS5_HOST"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("FTP_PORT", "2121")))
    args = parser.parse_args()
    if not args.host:
        parser.error("give --host or set PS5_HOST")
    root = pathlib.Path(__file__).resolve().parents[2]
    ftp = connect(args.host, args.port)
    try:
        if args.what == "title":
            deploy_title(ftp, pathlib.Path(args.path) if args.path else root / "dist" / "PPSA99256")
        else:
            if not args.path:
                parser.error("give the path to your libroblox.so")
            deploy_roblox(ftp, pathlib.Path(args.path))
    finally:
        try:
            ftp.quit()
        except ftplib.all_errors:
            ftp.close()
    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
