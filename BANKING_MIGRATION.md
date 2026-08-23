# MMC3 Migration Contract

## Rebuild Baseline

The active build now deliberately links only `src/v2_title_main.c`: title
presentation, its talking portrait, and the NMI-clocked music. The previous
adventure implementation remains in source control as reference, but is not
part of `game.nes`. New locations will be rebuilt one scene at a time against
the layout below rather than migrated as a live compatibility core.

## Why The Existing Image Is Not Yet PRG-Banked

`game.nes` has a 512 KiB PRG image, but the current executable is a 32 KiB
compatibility core placed into banks 0, 1, 62, and 63. The generated linker map
shows that the current runtime occupies addresses `$8000-$F62A`; therefore it
executes from both MMC3 switchable windows. Replacing either of those windows
from ordinary C code can remove the caller's return address and crash.

The extra PRG and CHR banks are a cartridge capacity, not a flat CPU address
space. Every banked call must originate from code that stays mapped for the
whole call and return path.

## Target Layout

| Region | Role | Rule |
| --- | --- | --- |
| PRG 62, `$C000-$DFFF` | fixed engine | NMI, mapper writes, OAM, input, save/game state, far-call trampolines |
| PRG 63, `$E000-$FFFF` | fixed boot | reset, vectors, shared emergency return stubs |
| PRG 0-61, `$8000-$BFFF` | banked content | scene update/render code, dialogue, hotspots, music data, scene-specific logic |
| CHR 0 | common graphics | font, UI, player, persistent effects |
| CHR 1 | title graphics | selected on title entry while rendering is disabled |
| CHR 2-31 | scene graphics | loaded on scene entry while rendering is disabled |

## Required Extraction Order

1. Move reusable engine primitives out of `main.c`: PPU queue, OAM, controller,
   palette, text, mapper control, and global game state.
2. Link that engine only into fixed PRG banks 30/31, with strict 16 KiB budgets.
3. Define a banked scene ABI: `enter`, `update`, `render`, and `leave` use RAM
   state only and return through a fixed-bank trampoline.
4. Move Neon Avenue, Bar, Sewer, Casino, title presentation, and their static
   data into independent PRG banks. Title CHR already selects bank 1 at
   runtime; its current contents are a compatible base copy pending art split.
5. Replace forced-blank direct writes with an NMI-owned VRAM command queue.
   Reel animation becomes a small queued tile update rather than a modal redraw.
6. Add a manifest that assigns every PRG and CHR asset to a bank, and make the
   build reject collisions or budget overruns.

## Current Guardrail

`build.bat` now emits `build/game_core.map`. Before moving any code, consult
that map rather than assuming a function is in a safe fixed window. The
migration is complete only when no gameplay call path depends on the current
32 KiB compatibility-core placement.

## Status

None of the six extraction steps above have started; Neon Avenue, the bar,
and the casino (see `REBUILD_ROADMAP.md`'s Present Slice) were all built
directly into the one 32 KiB compatibility core, same as the title screen.
The "CHR 2-31: scene graphics" row in the target layout table above is the
same destination `REBUILD_ROADMAP.md`'s "CHR Budget" section scopes a path
to -- that section has the concrete next steps and the current 256-tile
constraint's cause; this file has the target end state and extraction
order to reach it.
