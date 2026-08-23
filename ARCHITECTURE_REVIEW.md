# Architecture Review

Reviewed 2026-08-21. This is a code-and-documentation review of the
current NES ROM, not a replacement design. The project builds successfully
and its current `sim65` test suite passes. Findings are ordered by the risk
they pose to the next phases: more content, a dungeon, item gating, and
SRAM persistence.

## Executive assessment

The project has a sound early-game architecture. The most important choice --
keeping room, movement, font, and RNG logic independent of NES registers --
is working and is backed by real tests. The shadow-OAM DMA design is also the
right hardware boundary: sprite composition happens in RAM and only the
single DMA transfer has a vblank deadline.

The next scaling limit is not CPU speed. It is ownership: `src/main.c` now
owns authored world data, runtime game state, simulation rules, input policy,
PPU rendering, and transition exceptions. It is already 2,037 lines. Adding
Phase 6/7 systems directly there will make save data and content changes
increasingly fragile. The recommended direction is incremental extraction of
pure, testable game rules while preserving `main.c` as the NES shell.

## Current model

```
Input ($4016) -> main loop -> pure game rules -> runtime state
                       |                         |
                       |                         +-> OAM shadow buffer
                       v
                PPU / DMA / nametable writes

Authored data currently lives alongside the shell in main.c:
rooms, links, palettes, decorations, portals, items, NPCs, enemies, and
critters. Some tables are `const`; items, NPCs, enemies, and critters mix
authored fields with session state in the same records.
```

## Findings

### P1: World connectivity is duplicated without validation

`room_layouts[]` declares which border gaps are open while
`room_connections[]` separately declares where those gaps lead
([src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:369),
[src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:440)). The two
tables currently agree by authoring discipline alone. A new room can easily
have a walkable edge with no destination, or a destination behind a solid
border. Reciprocal links and destination IDs are also unchecked.

This is the first content-integrity problem to solve before expanding toward
40-80 rooms. Put room metadata and directional exits into a pure `world`
module, then add a `world_validate()` test that checks:

- each destination is `ROOM_NONE` or a valid room;
- each directed edge has a matching open exit in its source layout;
- reciprocal cardinal links use the opposite edge, except when explicitly
  marked one-way;
- portals have valid destinations and non-blocking source/destination cells;
- authored entities sit on valid, non-blocking cells.

The validator should permit the town and house one-way behavior deliberately,
rather than treating it as an unstructured exception.

### P1: Transition destination policy is split between data and control flow

Portals contain their own destination position, but cardinal room links hold
only a room ID. `main()` then special-cases town and house south exits with
literal coordinates ([src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:2000)). This
will grow linearly with every non-grid connection and is hard to serialize or
validate.

Replace `RoomConnections` with directional `RoomExit` records containing
`destination`, `spawn_x`, `spawn_y`, and flags such as `ONE_WAY`. A portal can
use the same destination-record type, plus its source trigger position. The
transition function then becomes data-driven: resolve a transition, set room
and spawn point, call `load_room()`. This removes room-ID-specific gameplay
logic from the main loop.

### P1: NPC room lookup is singular, but the data model permits co-location

`npc_in_room()` returns only the first NPC in a room
([src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:1259)). Rendering
does iterate all NPCs, but player collision and interaction use only that one
result ([src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:1849)).
The comment near OAM slots promises that multiple NPCs can coexist, while the
interaction layer cannot support it.

Before adding dense town/dungeon content, replace the single `npc_idx` path
with a small per-room query: find the nearest interactable NPC and build a
bounded obstacle list for collision. Start with a fixed capacity tied to the
NES sprite budget and test the tie-breaking policy. This also creates a clean
place to define whether dialogue pauses every NPC or only player input.

### P1: Quest generation has an implicit pool-size invariant

`generate_quests()` assigns unique items until `pool_count` is zero, then
silently leaves later fetch NPCs with their initial `item_id` of zero
([src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:885)). Today the
project has four fetch NPCs and four items, so the invariant holds. Adding a
fifth fetch giver without an item will create duplicate or misleading quests
instead of producing an obvious failure.

Make the generator return a success/failure result and test both valid and
insufficient-pool inputs. More importantly, move quest assignment into a
pure `quest.c` module operating on small IDs and records, with authored NPC
templates separate from runtime completion state. That module can later own
the save representation without dragging in PPU dependencies.

### P1: Collision/movement rules are already duplicated

`movement.c` has a private four-corner tile collision implementation, while
`main.c` repeats it as `enemy_tile_blocked()` for enemies and critters
([src/movement.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/movement.c:23),
[src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:948)). The copies
are currently equivalent, but changing sprite size, bounds, or obstacle rules
will require remembering both paths.

Extract the shared primitive as a hardware-independent API, for example
`box_hits_tiles(layout, x, y)` and `move_box(...)`. Keep player-specific
input decoding in `movement.c`; make enemy, critter, and NPC walkers call the
shared primitive. Add tests for all callers' edge behavior rather than
allowing behavior to drift by copy-paste.

### P2: NPC schedule routing silently assumes direct neighbors

`direction_to()` defaults to west if its destination is not a direct neighbor
([src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:1165)). The
existing schedule data is valid, but a future home/work pair farther apart
will move an NPC through the wrong exit and may index `ROOM_NONE` later.

Treat routes as authored data or have a pure route finder return `NO_ROUTE`.
At minimum, validate every scheduled NPC route at boot in debug builds. For
the planned village scale, an authored list of room IDs per schedule leg is
more predictable and cheaper than a general pathfinder.

### P2: Authored data and mutable state need separation before SRAM work

`Item`, `Npc`, `Enemy`, and `Critter` mix immutable placement/configuration
with session-only fields such as `taken`, `quest_complete`, HP, and wander
timers ([src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:759),
[src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:816),
[src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:913)). This is
convenient now but makes it unclear what Phase 7 should save, restore, or
reset on room entry.

Create `const` definitions and compact mutable state arrays keyed by stable
IDs. A useful first save contract is: a version byte; flags for one-time room
events/items/enemy clears; NPC quest state/disposition; player inventory,
weapon skills, HP, and spawn; and a checksum. Keep transient animation,
wander, and OAM state out of the save format. Versioning should exist from
the first save write, even for a one-slot save.

### P2: Palette allocation is manually coupled to room content

The four available sprite palettes are reused by role with documented
assumptions such as “no room has both an item and an enemy”
([src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:77)). Those
assumptions are already a content rule encoded in C comments. A dungeon room
with a pickup, enemy, sword, and NPC can break readability without any
compiler-visible error.

Declare palette needs in room/entity data and validate a room's active palette
set. In the short term, choose a deliberate palette contract (for example:
player, NPC, hostile, interactable) and let all items share the interactable
palette. Avoid adding more role aliases without documenting the conflict
matrix.

### P2: The random stream combines deterministic generation and simulation

The fixed-seed quest generator and wandering behavior both consume the one
global RNG stream ([src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:891),
[src/main.c](/C:/jbax-and-his-ai-pals/006-nes-game/src/main.c:975)). The current
boot-time generation happens before wandering, so it is deterministic today.
But any future runtime quest roll will become sensitive to unrelated ambient
movement and therefore hard to reproduce or save.

Change the RNG API to explicit state objects, or maintain separate streams:
one seeded world-generation stream and one simulation stream. Record or save
the world seed if generated assignments are meant to persist.

### P3: Build output does not expose NROM headroom

The generated ROM is the fixed 40,976-byte iNES size (16-byte header, 32 KiB
PRG, 8 KiB CHR), so its file size cannot show remaining NROM capacity. The
build script does not emit a linker map ([build.bat](/C:/jbax-and-his-ai-pals/006-nes-game/build.bat:7)).
This makes the mapper-migration decision reactive.

Emit and retain a `.map` file in builds, then add a lightweight budget check
or documented threshold for PRG, CHR, RAM, and OAM slots. Do this before the
content pass, while moving to MMC1/MMC3 is still a design choice rather than
a rescue operation.

## Recommended implementation order

1. Extract `world.h/.c`: room IDs, layouts, metadata, directional exits,
   portals, and a unit-tested `world_validate()`. Leave PPU palette and CHR
   selection in the renderer initially.
2. Extract `collision.h/.c`: four-corner tile collision and generic movement
   helpers. Convert player, enemy, critter, and NPC movement one at a time.
3. Extract `quest.h/.c`: quest assignment, readiness, dialogue selection
   inputs, and explicit generator failure. Add `test_quest.c`.
4. Replace `npc_in_room()` with bounded room queries and extract schedule
   routing. Add tests for direct, one-way, invalid, and multi-NPC cases.
5. Define `SaveData` and pure pack/unpack/checksum functions with simulated
   tests before touching battery-backed SRAM registers.
6. Add ROM/RAM/OAM budget reporting, then start the first dungeon and item
   gate on the new data model.

## Progress since review

The first extraction slice is complete:

- `src/quest.c` owns unique fetch-item assignment and fails atomically when
  the item pool cannot cover every fetch NPC; `tests/test_quest.c` covers
  success, uniqueness, and the insufficient-pool case.
- RNG now uses explicit `RngState` instances. Quest generation has its own
  deterministic stream, while combat and ambient movement use a separate
  simulation stream, preventing future runtime rolls from changing generated
  quest assignments.
- `box_hits_tiles()` is the shared four-corner tile-collision primitive for
  player, enemy, and critter movement, with direct coverage in
  `tests/test_movement.c`.

The world-transition slice is also complete: `src/world.c` validates that
room-border openings match destinations and that normal links round-trip;
explicit one-way flags cover town and house exits. Entry spawns now live in a
per-room, per-direction data table rather than `main()` conditionals.

Compact interior rooms are now a layout capability: `RoomLayout` can reserve
an inset dark surround while retaining a centered exit passage. This lets
houses feel intentionally small without needing full-screen furniture data.

The two current houses now use an 8x8 centered floor plan, a door portal on
its bottom edge, and black void everywhere else.

Their compact footprints now also use blocking wooden wall rings; the single
southern door tile is the only walkable cell on that wall.

Music timing now has an explicit hardware boundary. `src/startup.s` owns the
reset and vector code (based on cc65's NES startup), and its vblank NMI calls
the tiny assembly-only sequencer in `src/music.s`. This keeps the song at the
television frame rate even when `main.c` is busy rebuilding a room. The NMI
does not write PPU registers: this project performs direct PPU redraws while
forced blank, and an interrupt-side PPU reset could split one of those writes.
`main.c` only starts the theme; it never advances notes.

Dialogue overlays now restore only the nametable and attribute rows they
covered. `hide_text_box()` used to rebuild the full room attribute table while
rendering was disabled, including a 960-tile scan just to restore the bottom
four rows. `draw_room_attribute_row()` is the reusable bounded-redraw path:
new overlays should similarly restore only their own tiles and palette bytes.

Audio now has a small platform boundary in `src/music.h`: gameplay requests
named effects, while `src/music.s` owns the APU registers. The title theme
reserves pulse 1 and triangle; effects use pulse 2 and noise, so sword swings,
dialogue, pickups, hits, player damage, and door portals do not disrupt the
song. Effects with more than one note are advanced from NMI, never from the
game loop.

Noise effects must leave the APU length-counter halt bit clear. A persistent
transition hiss was traced to effect control bytes that accidentally set that
bit; each noise effect now uses a finite hardware length, so it cannot remain
audible after its event ends.

Combat recoil is now a pure movement capability. `knockback_step()` and
`knockback_direction_away()` in `src/movement.c` move one pixel at a time and
honor the same tile and screen-boundary rules as ordinary movement; focused
`sim65` coverage verifies direction selection and wall/edge stops. `main.c`
composes those steps into six-frame player and enemy knockback states, instead
of teleporting a hit target through geometry.

The gameplay shell now has explicit active, paused, and respawning modes in
`main.c`. The bounded pause panel owns Inventory and Quit actions; it restores
only its rows when dismissed. Quit calls `begin_new_game()`, which resets all
mutable pickups, quests, enemies, critters, and schedule state before the
title flow resumes. HP is redrawn after room loads and health changes, while a
zero-HP player is held in a short death state before a deterministic Meadow
respawn. Future save/load work should replace `begin_new_game()`'s reset
source with separate immutable definitions and per-session state arrays.

Title presentation now uses a localized attribute-table primitive,
`set_attribute_block()`, to give its license-plate panels a cream/red/navy
palette without recoloring the surrounding midnight field. This is preferable
to broad attribute-row changes for any future screen with layered panels or
badges: palette boundaries should follow the visual object, not the whole row.

The title portrait is a reproducible project asset rather than manually copied
tile bytes: `assets/title_portrait_reference.png` feeds
`tools/gen_title_portrait.py`, which emits a 10x12, four-color CHR sheet and
a pixel preview. The output includes a nine-tile closed-mouth replacement;
`wait_for_new_game()` queues that patch in a varied idle cadence; the assembly
NMI applies it at the start of vblank. It has title-only palettes, so
its skin, glasses, and separate light-gray beard highlights do not consume
gameplay palette slots. Keep portrait art under the remaining 256-tile
pattern-table limit and regenerate the assembly whenever its source or color
mapping changes. The adjacent speech plate uses the same NMI-owned approach:
C queues only a message index, while `assets/title_messages.txt` is converted
to the three padded 17-tile rows in `src/title_messages.s` during each build.

## Boundaries to preserve

- `main.c` should remain the NES platform shell: controller reads, vblank
  sequencing, PPU writes, DMA, and orchestration of pure modules.
- Keep NMI code short, assembly-only, and limited to frame/audio state. Do
  not call C from it or write PPU registers until rendering moves to a
  deliberately NMI-owned buffer system.
- `text.c` should remain a low-level PPU utility, not acquire quest or NPC
  knowledge.
- Keep collision, world validation, quests, schedules, combat resolution,
  and save packing free of `<nes.h>` and covered by `sim65` tests.
- Keep the OAM shadow buffer/DMA path. Do not reintroduce per-entity PPU OAM
  writes.
- Keep the current “small, observable slice” cadence from ROADMAP.md: each
  extraction should preserve a playable ROM and add a focused test suite.

## Verification checklist for each extraction

- `test.bat` passes, including new pure-module tests.
- `build.bat` produces a ROM and a linker map.
- In Mesen, verify room transitions, dialogue open/close, item pickup,
  quest completion, NPC movement, combat hit/miss, contact damage, HP/death
  respawn, pause menu actions, and the absence of stray sprites after a room
  transition.
- If a content invariant fails, surface it in a test or debug assertion;
  never rely only on a comment beside the data table.
