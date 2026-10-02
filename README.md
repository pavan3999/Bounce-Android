# Bounce Native Android

Native Android/NDK port scaffold for the uploaded Nokia Bounce J2ME build.

## Implemented in this revision

- Original `objects_nm.png` atlas loaded into native C++.
- Exact `Q[0..66]` construction translated from `com.nokia.mid.appl.boun.b.c()`.
- Original 12x12 tile dispatch for map IDs 0..54, including variant bit `0x40`.
- Original 16-entry terrain overlay tables from `d.class`.
- Native 156x96 off-screen playfield and 128x128 logical frame.
- Nearest-neighbor Android scaling for FHD/FHD+/QHD/UHD-class displays.
- All 11 original level files packaged as Android assets.
- Exact 12x12 and 16x12 collision masks extracted from `f.class`.
- Native player fixed-step movement/collision foundation.
- Nokia cheat sequences `787898` and `787899`, plus advanced `1/3/5/#` hooks.

## Not finished yet

The player update is intentionally marked as a native foundation rather than a claim of complete 1:1 physics. The remaining work is to translate the complete `f.b()` state machine, tile-specific collision switch, dynamic objects, camera/HUD, sounds, and level transitions.

This project does not include a J2ME runtime or emulator.
