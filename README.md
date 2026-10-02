# Bounce Native Android

Native Android/NDK port scaffold for the uploaded Nokia Bounce J2ME build.

## Implemented in this revision

- Original `objects_nm.png` atlas loaded into native C++.
- Native Q-table construction and transformed sprite reuse.
- Original map tile dispatch foundation for IDs 0..54 and variant bit `0x40`.
- Original terrain overlay tables.
- Native 156×96 off-screen playfield and 128×128 logical frame.
- Nearest-neighbor presentation through Android Canvas.
- All 11 original level files packaged as Android assets.
- Native level switching for advanced cheat `1` / `3`.
- Native dynamic-object state and 24×24 composed object rendering.
- Camera follows the native player in world coordinates and clamps to the level bounds.
- Resolution-independent touch coordinate conversion: FHD/FHD+/QHD/UHD displays feed 128×128 logical coordinates.
- Exact cheat sequence handling for `787898` and `787899`, with advanced `1`, `3`, `5`, `#` hooks.
- Native player collision/movement foundation using the extracted player masks.

## Not finished yet

This is still a development port, not a claim of 1:1 gameplay completion. Remaining work includes the complete player state machine, exact tile collision semantics, exact object behavior, player animation/state transitions, HUD, OTT audio playback, menus, persistence and final build validation on Android hardware.

This project does not include a J2ME runtime or emulator.
