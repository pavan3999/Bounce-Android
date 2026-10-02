# Bounce Native Android — port scaffold

This project is a native Android/NDK starting point built from the uploaded `Bounce_j2me.jar` data.

## Current milestone
- No J2ME runtime/emulator.
- Original level files/assets copied into Android assets.
- Native level header/map/object parser implemented.
- Original 128x128 logical frame and 128x96 playfield constants recorded.
- Exact six-key cheat state machine implemented:
  - 787898 -> invincible
  - 787899 -> advanced cheats
  - advanced 1 -> previous level
  - advanced 3 -> next level
  - advanced 5 -> invincible
  - advanced # -> player g=300
  - advanced GameAction 8 -> complete level

## Not yet complete
The renderer, player physics, object collision, sound decoder, and Android touch UI still need to be translated from the original bytecode. This is deliberately a native scaffold, not a claim that the game is already playable.

## Logical rendering target
128x128 final frame; 128x96 gameplay area; 32px HUD. Original offscreen playfield is 156x96.
