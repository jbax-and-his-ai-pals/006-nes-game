# Current Architecture

## Authoritative Runtime

The active game is the v2 rebuild, not the archived action-RPG prototype.
`build.bat` builds the fixed engine plus each manifest-owned scene module,
then packs them into the MMC3 cartridge. `src/main.c`,
`movement.c`, `room.c`, and related systems remain useful historical
reference and unit-test fixtures, but they do not ship in `game.nes`.

The playable flow is:

1. Title presentation and portrait dialogue.
2. Neon Avenue, including the taxi death loop and entrance to Jimbles's Bar.
3. All-Nite Mart exterior, reached through Neon Avenue's western edge, with
   its own taxi death loop. Its separate store interior has shelf collision,
   shelf/cashier/register interactions, and the persistent paper-clip pickup.
4. A walk-over manhole and first sewer chamber, with a climb-up return to
   Neon Avenue.
5. Jimbles's Bar, including token, jukebox, bartender/patron dialogue, and
   the casino door, plus the Piss Corner's roaming banished patrons and
   evolving dialogue, plus a side door to the Commentarium.
6. Wrecked 'Em Casino, including slots, blackjack, video poker, roulette, and
   a walked exit.
7. Moonlight Park, reached through Neon Avenue's eastern edge, with a dry
   fountain, trees, wandering patrons, and its own active CHR page.
8. Pocket Zoo, reached through the sewer's Zoo service hatch, with the slug
   infestation, Keeper Nibs, the Zoo Map, and the pond/reptile exhibits.
9. The Commentarium, reached through Jimbles's Bar's side door: a stage
   performer and a rotating feed of comedic "dumb comments" pulled from a
   data file (see `src/dumb_comments_data.h` and `tools/gen_dumb_comments.py`).
10. A Pawn Shop/Laundromat side street, reached through the Mart's western
    edge (the Mart's own west boundary exits here instead of just stopping
    the player). Two exteriors on one screen, both doors reading "not open
    yet"; the gap between them leads north to Galaxy Lanes & Arcade, capped
    by a collidable horizon line rather than an open placeholder.
11. Galaxy Lanes & Arcade exterior, reached through the side street's north
    path. Exterior only for now -- the door reads "not open yet"; see
    `ROADMAP.md`'s Town Blueprint for the planned interior.

## Cartridge and Bank Baseline

`tools/pack_mmc3.py` produces an iNES mapper-4 cartridge with 512 KiB PRG
and 256 KiB CHR. The reset stub in the fixed final PRG bank maps the current
runtime into MMC3's initial CPU windows. CHR mapping is explicit rather than
depending on mapper power-on state.

The live cartridge uses the fixed engine in PRG banks 62-63 and maps each
location into `$8000-$BFFF` from its manifest-owned PRG pair. The current
scene pairs (see `assets/banks/manifest.json`) are Park 03-04, Avenue 05-06,
Bar 07-08, Mart 09-10, Sewer 11-12, Casino 13-14, Zoo 15-16, Commentarium
17-18, Music 19-20, Arcade 21-22, and the Pawn Shop/Laundromat side street
23-24; PRG banks 25-61 remain available for future destinations.
`V2GameState.route` records the active location and spawn; see `ROUTING.md`
when adding exits, respawns, or destinations.

A new scene doesn't have to hang off Neon Avenue directly -- the side street
extends the *Mart's* own west boundary instead of adding a new Avenue exit,
and the arcade extends the *side street's* own north boundary the same way,
same as the Commentarium extends the Bar's. Each is a one-line dispatcher
addition in `src/v2_title_main.c` plus a `fixed_scene_stubs.c` wrapper (see
`run_arcade_exterior`'s comment there) and a `SCENE_STUBS` entry in
`tools/gen_fixed_api.py`, not new Avenue-level plumbing.

## Fixed-Core Responsibilities

`src/v2_runtime.c` owns the controller shift-register read and the page-
aligned shadow OAM buffer/DMA transfer. New scenes should use this interface
rather than directly reading `$4016` or triggering `$4014` DMA.

`src/v2_vram_queue.c` provides a bounded 32-tile command queue. Its entries
are written by C between frames and consumed by NMI during vblank. The casino
HUD and message-panel row construction use it; full-scene construction and
palette changes still use the forced-blank path. The slot reel animation
advances one stage every three frames and locks slot input until all three
displayed reels settle on the already-resolved result. Dialogue text types in
one queued tile per frame.

`src/v2_game_state.c` owns persistent inventory, interaction, character, and
casino state (see `src/v2_casino.c` below for the casino's own rules). All
casino credit math saturates at 255, leaving the eventual maximum-credit
event as a content decision instead of allowing numeric wraparound.

`src/startup.s` owns reset, NMI, music tick, initial MMC3 mapping, and title
message/mouth updates. `src/mmc3.h` exposes only CHR mapping because C code
must not remap the PRG window that contains its own return address.

`src/v2_scene_ui.c` owns scroll reset plus generic tile, rectangle, attribute,
and text writes. `src/v2_dialogue.c` owns the typed, three-line message overlay
and its OAM clipping/NMI-queue protocol. Together these are the first extracted
fixed-engine candidates; scene loops select palettes and redraw their own
background after a message closes.

`src/v2_input.c` owns held-button debouncing, while `src/v2_inventory.c` owns
the inventory modal and descriptions. These services use only game state and
the shared UI/runtime APIs, so they can move to the fixed engine without a
scene-specific dependency.

`src/v2_scene_casino.c` owns the complete casino location loop -- slots,
blackjack, video poker, roulette, and the walkable lobby -- as its own PRG
scene pair (13-14, see above), not the mapped runtime. `src/v2_casino.c`
(the hardware-independent game rules: wild-symbol slots, single-deck poker,
natural blackjack, roulette payouts) lives in the same scene pair.

## Present Constraints

Each of the eight scene pairs above is a real, independently-linked PRG
module built via its own `build_scene_*.bat` and checked by
`tools/check_scene_module.py`; the fixed-bank dispatcher and trampoline this
section used to describe as upcoming work (see `BANKED_DISPATCHER.md`) are
done and in daily use. What's still true and worth knowing before adding to
either budget:

- **Two separate, unrelated budgets.** The fixed engine (PRG banks 62-63,
  built by `build_fixed_engine.bat`) is a single shared 16350-byte pool used
  by every scene; its own build output reports current usage and headroom.
  Each scene's own PRG pair is a *separate* 16 KiB ($4000-byte) budget for
  that scene's CODE + RODATA + any scene-owned data segment (e.g. the
  casino's `SCENE_CASINO_DATA`); `build_scene_<name>.bat`'s ld65 output (or
  a failed link with an "overflows memory area" warning) is the source of
  truth for a specific scene, not the fixed engine's number. A scene running
  low on its own budget cannot borrow from the fixed engine or from another
  scene's pair.
- The NMI queue handles small tile changes and panel rows, but palette/fill
  commands and larger scene transitions still force blanking.
- The visible 256-tile pattern-table half (the common CHR bank) is full.
  Casino/card art and the sewer each already have their own dedicated CHR
  bank (`casino_cards.s` / `sewer_chr.bin`); a new destination needing
  substantial new background art should get one too rather than trying to
  fit into the common bank. When genuinely no CHR slot exists, byte-for-byte
  duplicate tiles across the common bank's source files are sometimes
  reclaimable (a full scripted duplicate scan can find them) -- but this
  changes existing art or strings elsewhere, so treat it as a last resort
  and say plainly what's being reused for what.

## Near-Term Order

1. Playtest the Zoo and Commentarium end to end from a clean boot, the same
   scrutiny already applied to Avenue/Bar/Mart/Sewer/Park/Casino.
2. Keep `ROADMAP.md`'s town blueprint and phase checklist in sync with what
   actually ships here -- it drifted significantly behind this file and
   `WORLD_MAP.md` before its latest pass.
3. Extend the NMI queue with palette/fill commands for richer UI and smoother
   scene transitions.
4. Decide the next destination via `WORLD_MAP.md`'s expansion order, and give
   it its own CHR bank up front if it needs real new art.
