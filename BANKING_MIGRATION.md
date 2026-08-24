# MMC3 Migration Contract

## Rebuild Baseline

The active build links the rebuilt scene dispatcher, shared input/OAM/UI
services, title presentation, and NMI-clocked music. The previous adventure
implementation remains in source control as reference, but is not part of
`game.nes`. New locations will be rebuilt one scene at a time against the
layout below rather than expanded as one contiguous runtime.

## Why The Existing Image Is Not Yet PRG-Banked

`game.nes` has a 512 KiB PRG image, but the current executable is a 32 KiB
runtime image placed into banks 0, 1, 62, and 63. The generated linker map
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
2. Link that engine only into fixed PRG banks 62/63, with strict 16 KiB budgets.
3. Define a banked scene ABI: `enter`, `update`, `render`, and `leave` use RAM
   state only and return through a fixed-bank trampoline. The descriptor must
   identify the first bank of a contiguous two-bank PRG pair, not a single
   bank.
4. Move Neon Avenue, Bar, Sewer, Casino, title presentation, and their static
   data into independent PRG banks. Title CHR already selects bank 1 at
   runtime; its current contents are a stable base clone pending art split.
5. Replace forced-blank direct writes with an NMI-owned VRAM command queue.
   Reel animation becomes a small queued tile update rather than a modal redraw.
6. Add a manifest that assigns every PRG and CHR asset to a bank, and make the
   build reject collisions or budget overruns.

## Current Guardrail

`build.bat` now emits `build/game_runtime.map`. Before moving any code, consult
that map rather than assuming a function is in a safe fixed window. The
migration is complete only when no gameplay call path depends on the current
32 KiB mapped-runtime placement.

## Status

The build now has three migration guardrails: it reports and enforces a small
mapped-runtime headroom budget, and `assets/banks/manifest.json` owns the first
real banked PRG payload (casino scene data in bank 2). Shared UI, dialogue,
input, inventory, actor, traffic, regular, and game-state services are now
fixed-engine candidates. All locations still execute from the one temporary
32 KiB mapped runtime; no banked executable scene call exists yet because the
fixed-bank dispatcher is not live. Moonlight Park safely selects CHR bank 2 at
runtime; the bank starts as a common-graphics clone and is available for
Park-only art.

Input debouncing and the inventory modal live in `v2_input.c` and
`v2_inventory.c`; they depend only on fixed-engine candidates and game state.
The temporary mapped runtime leaves 181 bytes free, while the future fixed
engine has its own independent 2 KiB-reserve guard.

All current locations now have source-level scene boundaries:
`v2_scene_avenue.c`, `v2_scene_bar.c`, `v2_scene_casino.c`,
`v2_scene_park.c`, `v2_scene_mart.c`, and `v2_scene_sewer.c`. Their room
rendering, collision, interactions, minigames, traffic flow, inventory grants,
and exits now live outside `v2_title_main.c`. They remain linked into the
mapped runtime until the fixed-bank dispatcher can invoke their assigned PRG
pairs safely.

The latest linker map measures `v2_title_main.c` at roughly 1.2 KiB of code;
`v2_scene_casino.c` is roughly 5.8 KiB and `v2_scene_bar.c` roughly 2.7 KiB.
The taxi/death presentation and the shared Bar/Park regular actor live in
`v2_traffic.c` and `v2_regulars.c`, fixed-engine candidates shared by their
respective scenes. Startup/reset, NMI/title-queue helpers, music, title and
dispatch, and shared C services now occupy explicit `FIXED_ENGINE` linker
segments. The build enforces a 2 KiB reserve in the future 16 KiB fixed window.
`build_scene_park.bat` now also links Moonlight Park as a real 16 KiB scene
image at `$8000`, resolving calls through an API table generated from the
fixed-engine map. `tools/check_scene_module.py` rejects a module whose entry
is not exactly `$8000` or whose payload is not one full PRG pair. The remaining
critical step is to make the production dispatcher call that module; a partial
far-call remains unsafe. `build/fixed_layout.nes` already provides a manual
smoke route after title start: its Avenue validation stub selects PRG 05-06 and
calls the real Avenue via the fixed trampoline. This validates the mapper call path
without changing the stable playable `game.nes` dispatcher.

All current destinations now have independently linked, full-pair PRG images:
Park 03-04, Avenue 05-06, Bar 07-08, Mart 09-10, Sewer 11-12, and Casino
13-14. The remaining migration work is exclusively the production dispatcher
and return-route state machine; there are no remaining monolithic scene links.
The "CHR 2-31: scene graphics" row in the target layout table above is the
same destination `REBUILD_ROADMAP.md`'s "CHR Budget" section scopes a path
to -- that section has the concrete next steps and the current 256-tile
constraint's cause; this file has the target end state and extraction
order to reach it.
