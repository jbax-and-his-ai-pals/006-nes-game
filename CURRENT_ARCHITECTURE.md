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
   evolving dialogue.
6. Wrecked 'Em Casino, including slots, blackjack, video poker, and a walked
   exit.
7. Moonlight Park, reached through Neon Avenue's eastern edge, with a dry
   fountain, trees, wandering patrons, and its own active CHR page.

## Cartridge and Bank Baseline

`tools/pack_mmc3.py` produces an iNES mapper-4 cartridge with 512 KiB PRG
and 256 KiB CHR. The reset stub in the fixed final PRG bank maps the current
runtime into MMC3's initial CPU windows. CHR mapping is explicit rather than
depending on mapper power-on state.

The live cartridge uses the fixed engine in PRG banks 62-63 and maps each
location into `$8000-$BFFF` from its manifest-owned PRG pair. The current
scene pairs are Park 03-04, Avenue 05-06, Bar 07-08, Mart 09-10, Sewer 11-12,
and Casino 13-14. `V2GameState.route` records the active location and spawn;
see `ROUTING.md` when adding exits, respawns, or destinations.

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
casino state. `src/v2_casino.c` owns the hardware-independent slot rules and
saturates credits at 255, leaving the eventual maximum-credit event as a
content decision instead of allowing numeric wraparound.

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

`src/v2_scene_casino.c` now owns the complete casino location loop, including
slots, blackjack, poker, and its scene rendering. It currently links into the
mapped runtime for behavioral stability, but its dependencies are limited to
the extracted shared modules and casino state/data, making it the first PRG
scene-pair candidate.

## Present Constraints

- `v2_title_main.c` now owns only the title presentation and high-level scene
  dispatch. Neon Avenue, Jimbles's Bar, Casino, Moonlight Park, All-Nite Mart,
  and the sewer are source-isolated scene modules; their generic dependencies
  live in shared fixed-engine candidates. The next requirement is the
  fixed-bank dispatcher that makes those PRG pairs callable.
- The NMI queue handles small tile changes and panel rows, but palette/fill
  commands and larger scene transitions still force blanking.
- The visible 256-tile pattern-table half is full. Use the independent
  background/sprite pattern-table halves and then scene-specific CHR banks
  before adding substantial new art.
- A true PRG-banked scene entry and fixed-bank trampoline now build and pack;
  the remaining task is routing the title/game dispatcher through them.
- Startup/reset, NMI/title queue helpers, and music are explicitly assigned to
  transitional `FIXED_ENGINE` linker segments. They still share the temporary
  mapped window today, but are now ready to relocate together into fixed PRG
  banks without leaving an NMI call into switchable code.
- `mmc3_select_prg_pair()` now lives in that fixed segment and maps a
  contiguous scene pair into `$8000-$BFFF`. The next implementation step is
  the fixed trampoline described in `BANKED_DISPATCHER.md`.
- `build_fixed_engine.bat` proves the fixed engine links at its final
  `$C000-$FFFF` address range with a separate link-only scene-stub target.
- `build_fixed_layout.bat` proves that the fixed engine packs into PRG banks
  62-63 alongside Park's real executable pair at PRG 3-4.
- The current mapped-runtime guard reports 181 bytes free. The extraction work
  improves the eventual split but does not create room for new in-window
  features; avoid adding large functionality before the first PRG scene pair
  is callable.

## Near-Term Order

1. Split the current scene implementations around the new shared runtime.
2. Add pure-C tests for rebuilt gameplay rules such as taxi/death state
   transitions and Mart interaction/collision bounds.
3. Extend the NMI queue with palette/fill commands for richer UI and smoother
   scene transitions.
4. Route Moonlight Park through the fixed-bank trampoline, then apply the
   same module build contract to Neon Avenue and the Bar.
