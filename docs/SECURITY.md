# Security

## Reporting a vulnerability

Use a private security advisory on this repository
(Security → Advisories → Report a vulnerability). Do not open a public issue
for it.

## What the project protects, and how

**Accounts.** Nothing that could be a credential is logged: every log line
passes through `akeno_log_redact`, which masks `.ROBLOSECURITY` values,
`password=`/`token=`/`secret=`/`session=` values, and everything after an
`Authorization:` or `Cookie:` header (tested in `tests/unit/test_settings.c`).
When sign-in exists, a session will not be saved unencrypted: the console has
no keyring a title is known to be able to use, so sessions will be kept in
memory only until secure storage is found and verified. Playden's choice to
store a token unencrypted is the pattern not to follow.

**Untrusted files.** A user-supplied `libroblox.so` is validated by a parser
that bounds every offset and size before anything is mapped; the loader
refuses malformed files rather than faulting, and both are fuzzed
(`tests/fuzz/`, ASan and UBSan). Settings files are parsed line by line with
fixed buffers; unknown keys are ignored, out-of-range values clamped.

**The console.** The title writes only under its own storage directory
(`/data/akenoblox` or `/download0/akenoblox`) and its title folder; it opens no
listening sockets; it makes one DNS query and one local-only UDP loopback round-trip (for the
Diagnostics network rows) when asked. It does not send UDP traffic to external
servers. Exported reports redact the stack-protector canary value. It does not elevate itself, load payloads, or touch the kernel.

**Roblox.** The project never patches, hooks or injects into the engine,
never spoofs device integrity, never forges tokens and never rewrites the
engine's traffic (ADR-0008). Executable memory holding loaded code is
read-execute, not writable, wherever the console allows it; IFUNC resolvers
are refused unless explicitly allowed.

## What it does not protect against

Anyone with file access to a jailbroken console can read and change
everything on it, including AkenoBlox's files. Roblox does not support
third-party clients and bans accounts that use them; no part of this project
reduces that risk.
