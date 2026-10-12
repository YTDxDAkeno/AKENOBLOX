#!/usr/bin/env bash
# AkenoBlox PS5 - the experiment behind docs/FEASIBILITY.md section 3.6: does
# a Rust std staticlib built for x86_64-unknown-freebsd link into a console
# program, and what TLS model does it carry? Links only; runs nothing.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   probe-rust-ps5.sh <payload SDK> [work dir]
set -euo pipefail
sdk=$(cd "$1" && pwd)
work=${2:-$(mktemp -d)}
mkdir -p "$work/src"
rustup target add x86_64-unknown-freebsd > /dev/null
cat > "$work/Cargo.toml" <<'TOML'
[package]
name = "akeno_rust_probe"
version = "0.1.0"
edition = "2021"
[lib]
crate-type = ["staticlib"]
path = "src/lib.rs"
[profile.release]
panic = "abort"
TOML
cat > "$work/src/lib.rs" <<'RS'
use std::collections::HashMap;
use std::sync::{Arc, Mutex};
#[no_mangle]
pub extern "C" fn akeno_rust_probe(n: u32) -> u64 {
    let m = Arc::new(Mutex::new(HashMap::new()));
    let mut threads = Vec::new();
    for t in 0..4u32 {
        let m = m.clone();
        threads.push(std::thread::spawn(move || {
            for i in 0..n { m.lock().unwrap().insert(t * 1_000_000 + i, i as u64); }
        }));
    }
    for t in threads { t.join().unwrap(); }
    let total: u64 = m.lock().unwrap().values().sum();
    total + format!("{n}").len() as u64
}
RS
printf '#include <stdint.h>\n#include <stdio.h>\nuint64_t akeno_rust_probe(uint32_t);\nint main(void){printf("%%llu\\n",(unsigned long long)akeno_rust_probe(10));return 0;}\n' > "$work/main.c"
(cd "$work" && cargo build -q --release --target x86_64-unknown-freebsd)
lib="$work/target/x86_64-unknown-freebsd/release/libakeno_rust_probe.a"
"$sdk/bin/prospero-clang" "$work/main.c" "$lib" -Wl,--gc-sections -lunwind -lpthread -o "$work/probe.elf" \
    > "$work/link.log" 2>&1 || true
echo "undefined at link:"
grep -o 'undefined symbol: [^ ]*' "$work/link.log" | sort -u || echo "  none"
mkdir -p "$work/objects" && (cd "$work/objects" && ar x "$lib")
echo "native TLS relocations in the staticlib (the console's TLS is emulated):"
for object in "$work"/objects/*.o; do readelf -rW "$object" 2>/dev/null; done |
    grep -oE 'R_X86_64_(TLSGD|TLSLD|DTPOFF32|GOTTPOFF|TPOFF32)' | sort | uniq -c || echo "  none"
