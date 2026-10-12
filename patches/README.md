# Patches

Changes to third-party sources this project builds but does not fork, applied
by the build to a copy of the pinned source, never to a checkout: the
arrangement Cordial uses for the same AOSP linker (`patches/apply.rs` there).

Empty so far. The AOSP linker port's patches will land here, against the
`mcpelauncher-linker` revision Cordial pins (`1ac3ea6`): the four compile
fixes and the copy-load path described in
[docs/PORTING_NOTES.md](../docs/PORTING_NOTES.md).
