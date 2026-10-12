#!/usr/bin/env python3
# AkenoBlox PS5 - capture the console's kernel log while AkenoBlox runs.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   klog.py [--host H] [--port 3232] [--seconds N] [--out file] [--all]
#
# klogsrv (ps5-payload-dev) streams klog over TCP, port 3232 by default. The
# launcher's standard error reaches klog with an "[AkenoBlox] " prefix
# (ps5platform/klog.h), and so does every Diagnostics step: what a hardware
# test report needs. By default only those lines and crash records are kept;
# --all keeps everything.
#
# NOT YET RUN AGAINST A CONSOLE. Report what it does (docs/HARDWARE_TESTING.md).
import argparse
import datetime
import os
import socket
import sys
import time

KEEP = ("[AkenoBlox]", "signal", "crash", "SIGSEGV", "SIGSYS", "SIGILL", "SIGBUS", "PPSA99256")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=os.environ.get("PS5_HOST"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("KLOG_PORT", "3232")))
    parser.add_argument("--seconds", type=float, default=0, help="stop after this long (0: Ctrl-C)")
    parser.add_argument("--out", default="akenoblox-klog.txt")
    parser.add_argument("--all", action="store_true")
    args = parser.parse_args()
    if not args.host:
        parser.error("give --host or set PS5_HOST")
    deadline = time.monotonic() + args.seconds if args.seconds > 0 else None
    with socket.create_connection((args.host, args.port), timeout=10) as sock, \
            open(args.out, "w", encoding="utf-8", errors="replace") as out:
        sock.settimeout(1.0)
        pending = b""
        print(f"capturing klog from {args.host}:{args.port} into {args.out}", file=sys.stderr)
        try:
            while deadline is None or time.monotonic() < deadline:
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                pending += chunk
                *lines, pending = pending.split(b"\n")
                for raw in lines:
                    line = raw.decode("utf-8", errors="replace").rstrip("\r")
                    if args.all or any(key in line for key in KEEP):
                        stamped = f"{datetime.datetime.now().isoformat(timespec='milliseconds')} {line}"
                        print(stamped)
                        out.write(stamped + "\n")
                        out.flush()
        except KeyboardInterrupt:
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
