# Current Architecture

## Authoritative Runtime

The active game is the v2 rebuild, not the archived action-RPG prototype.
`build.bat` links `src/v2_title_main.c`, `src/v2_runtime.c`, the shared
assembly graphics/music sources, and the casino data module. `src/main.c`,
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
6. Shady Ass Casino, including the slot interaction and walked exit.

## Cartridge and Bank Baseline

`tools/pack_mmc3.py` produces an iNES mapper-4 cartridge with 512 KiB PRG
and 256 KiB CHR. The reset stub in the fixed final PRG bank maps the current
runtime into MMC3's initial CPU windows. CHR mapping is explicit rather than
depending on mapper power-on state.

This is **large-cartridge support**, not yet a banked-scene runtime. The
linked executable still fits in one 32 KiB compatibility core, and
`assets/banks/manifest.json` has no allocated scene payloads. This is an
intentional, safe foundation: scenes can now grow into the cartridge without
changing its file format, mapper, boot flow, or save-RAM layout.

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

## Present Constraints

- `v2_title_main.c` still owns scene logic and direct scene rendering. Split
  it into shared scene helpers plus separate scene modules before adding many
  destinations.
- The NMI queue handles small tile changes and panel rows, but palette/fill
  commands and larger scene transitions still force blanking.
- The visible 256-tile pattern-table half is full. Use the independent
  background/sprite pattern-table halves and then scene-specific CHR banks
  before adding substantial new art.
- A true PRG-banked scene needs a fixed-bank trampoline and an ABI that never
  lets banked C code select or return through a replaced PRG window.

## Near-Term Order

1. Split the current scene implementations around the new shared runtime.
2. Add pure-C tests for rebuilt gameplay rules such as taxi/death state
   transitions and Mart interaction/collision bounds.
3. Extend the NMI queue with palette/fill commands for richer UI and smoother
   scene transitions.
4. Introduce fixed-bank PRG trampolines, then assign an explicit PRG/CHR
   budget to the first banked destination in the manifest.
