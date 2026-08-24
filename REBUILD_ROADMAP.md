# V2 Rebuild Roadmap

`game.nes` is now a 512 KiB MMC3 baseline with a title screen, a full Neon
Avenue exterior, Jimbles's Bar interior, and a working casino. The original
adventure sources are retained for reference, but they are intentionally not
linked. This lets new work use the larger cartridge from the start rather
than inherit the old 32 KiB runtime's layout and collision assumptions.

## Present Slice

- Title presentation, portrait speech, mouth animation, and title music.
- Common graphics in CHR bank 0 and a title-graphics clone in CHR bank 1 selected
  at title entry.
- Fixed mapper boot in PRG bank 63 and the small active runtime packed into
  the initial MMC3 mapping.
- Neon Avenue exterior: a narrower building (a margin on each side, left
  open for future content) with a redesigned brick/sign/window/door facade,
  sidewalk/road staging, a six-sprite player, bounded movement, a taxi death
  loop, and a walk-over manhole route.
- A first sewer chamber below Neon Avenue: ladder entrance/return, dry-floor
  exploration space, and a southern water boundary. It intentionally reuses
  shared tiles under a damp palette until scene-specific CHR art is available.
- All-Nite Mart: a dedicated exterior one screen west of Neon Avenue's bar
  frontage, entered by walking through its western edge. It has the same
  directional taxi/traffic death trap as the Bar exterior. Its interior has
  collision-aware shelves, rotating shelf observations, a cashier dialogue,
  a separate register hotspot, and a persistent paper-clip pickup.
- Jimbles's Bar interior: a thickened north wall that fits the backroom
  door, counter/shelf collision, a walk-over token pickup, a jukebox that
  unlocks the backroom door (with a locked-door message if you try it
  first), Jimbles (an idle side-to-side slide animation) and a taller
  two-tile bar patron, both with one-time dialogue. The lower-right Piss
  Corner contains two independently wandering banished regulars, evolving
  dialogue, puddles/streaks, and a seated bar patron/stool read at the counter.
- Both exterior traffic traps use the same tall, checkered yellow taxi, a
  directional crash scatter, a persistent bones/head tableau, and a single
  standardized red death-message plate.
- The casino: a centered slot machine and a blackjack table (visual only, no
  game logic yet), reached through the bar's unlockable backroom door and
  exited through its own walked door back to the bar (not a button). The
  slot machine's UI (reel, credits, bet, controls) stays hidden until the
  player actually interacts with it, drawn in a dedicated dark overlay panel
  rather than sitting directly on the room's own floor palette. Reels now
  visibly advance and settle in sequence through NMI-owned HUD updates.
- A reusable bordered message box (`show_message_box()`), built from the
  same rule-line tiles the title screen's own dialogue plate uses, with a
  sustained-release input debounce so a genuinely held button doesn't
  re-trigger it or instantly dismiss it. Its static frame is forced-blank,
  while its text types in through the NMI queue.
- Shared v2 runtime/state modules now own controller/OAM service, persistent
  cross-scene state, and slot payout rules; casino credits saturate at 255
  instead of wrapping while a future maximum-credit event is still undecided.

## Destination Roadmap

The town should feel like one odd, walkable night out: irreverent late-night
venues and comic interactions in an original setting, without copying any
specific characters, plot, dialogue, or puzzles from its inspirations.

| Destination | Role and visual direction | Signature interaction | Build order |
| --- | --- | --- | --- |
| **All-Nite Mart** | Compact, overlit convenience store with crowded shelves and a tired clerk. | Buy, inspect, and combine small practical objects; first inventory-heavy puzzle. Exterior/interior route is built; content pass pending. | Current next pass |
| **Piss Corner** | Lower-right nook inside Jimbles's Bar, with yellow puddles/streaks and banished regulars. | Evolving barks, comic consequences, and a future hidden clue/shortcut. | Built; content expansion pending |
| **Moonlight Park** | Dark municipal park with a fountain, paths, benches, and an after-hours gate. | Environmental observation, a timed light/fountain gag, and NPC encounters. | Early |
| **Starlight Lodge** | Hotel lobby, elevator, hallway, and one restricted room; warm brass against deep blue night. | Social access puzzle, room key, and a compact multi-room transition test. | Early-mid |
| **Galaxy Lanes & Arcade** | Public bowling/arcade venue, distinct from the bar/casino nightlife. | Scoreboard, token, and simple skill-or-pattern minigame; a lively gathering place for clues. | Mid |
| **Slug Acres** | Absurd suburban slug farm: damp beds, irrigation pipes, and a tiny office. | Creature-care/inventory comedy; obtain a slippery or luminous puzzle resource. | Mid |
| **Pocket Zoo** | Small municipal zoo with exaggerated enclosures, keeper booth, and nocturnal exhibit. | Animal behavior puzzle and character-driven clue exchange. | Mid-late |
| **Municipal Research Annex** | Clinical laboratory at the end of a service corridor, with glassware and humming equipment. | Combine evidence/items, unlock the town mystery, and support the finale. | Late |
| **Sewer network** | Expand the current first chamber into a service route beneath the town. | Shortcut, gated maintenance paths, and clues connecting park, zoo, farm, and laboratory. | Incremental |

### Suggested Story Route

1. Start at Jimbles's Bar, then use the casino/token loop and Piss Corner's
   banishment gag to establish the town's interaction language.
2. Open All-Nite Mart and Moonlight Park as the first two outward destinations;
   together they support the first three-step recoverable puzzle.
3. Add Starlight Lodge and Galaxy Lanes & Arcade for a denser social hub and
   reusable NPCs.
4. Reveal Slug Acres, Pocket Zoo, and a larger sewer network as the stranger
   outer-town layer.
5. Use the Municipal Research Annex as the convergence point for collected
   clues and the endgame reveal.

### Implementation Rules For Destinations

- Every location needs one clear visual silhouette, one recurring character or
  object, one signature interaction, and at least one reason to return.
- Build All-Nite Mart, park, and hotel only after the active scene code is
  split or a small common scene layer exists; do not keep adding large loops
  to `v2_title_main.c`.
- New art-heavy locations require the background/sprite pattern-table split
  and an explicit CHR budget before implementation.
- The sewer can reuse existing art while its layout is small, but any expanded
  network gets its own scene CHR allocation rather than borrowing bar tiles.

## CHR Budget

The 256-tile pattern-table window every current scene shares (see
`MAPPER.md`) is fully allocated -- confirmed via `build/game_runtime.map`,
every one of the 256 tiles in CHR bank 0's first half is claimed by a
linked `.s` file, none free. Everything built in this slice (bar collision
zones, the casino's blackjack table, the taller patron, Jimbles's
animation) reused existing tiles rather than drawing new art, for exactly
that reason.

Two known, not-yet-taken paths to more room, in increasing order of effort:

1. **Reclaim ~10 tiles** from confirmed-dead legacy files still linked in
   `build.bat` (`npc.s`, `item.s`, `enemy.s`, `sword.s`, `critter.s`,
   `portal.s`) -- unreferenced by any v2 code, pure link-order dead weight.
   Small and bounded; requires renumbering every downstream tile constant
   by hand, the same class of mistake that caused this session's tile-
   alignment corruption bug, so do it as its own careful pass.
2. **Split each bank into a background half and a sprite half** via
   `PPU.control`'s two independent pattern-table-select bits (currently
   both pinned to the low half at boot and never touched again). Roughly
   doubles the ceiling with no new banking work. Then extend
   `mmc3_use_common_chr()`/`mmc3_use_title_chr()`/`mmc3_use_park_chr()`'s pattern into a
   parameterized per-scene bank select, using the CHR banks 3-31 still
   available below and the manifest system already wired into
   `pack_mmc3.py` (`assets/banks/manifest.json`, with Park owning CHR 2), so each
   scene(-group) gets its own fresh 256+256 budget instead of competing
   for one shared pool. The open question here is the CHR-authoring
   pipeline for banked content -- the manifest expects a pre-built raw
   binary per bank, not a `.s` file compiled into the shared core the way
   every tile today is authored.

The intent going forward is to actually grow into the cartridge's real
capacity (512 KiB PRG, 256 KiB CHR -- already larger than *Super Mario
Bros. 3*'s entire cartridge) rather than continue minimizing tile usage as
the default assumption.

## Rebuild Order

1. Establish a fixed-bank engine: NMI-owned VRAM queue, controller
   sampling, OAM DMA, mapper calls, and shared game state. **Started:**
   `v2_runtime.c` now owns controller sampling and the shared shadow-OAM/DMA
   path; `v2_vram_queue.c` now queues bounded tile updates for NMI, with the
   casino HUD as its first client; and `v2_game_state.c` owns shared state.
   PRG-bank trampolines and a fuller queue (rows/fills/palettes) are still
   pending. Large scene redraws and panel changes remain direct forced-blank
   writes from the game loop.
2. Extend the rebuilt Neon Avenue scene with a fixed-bank scene ABI, a real
   bar-door transition, and the taxi death trap. **Door transition and taxi
   death trap done**; the fixed-bank scene ABI itself is not started -- all
   three scenes still live in the one 32 KiB mapped runtime window.
3. Rebuild Jimbles's Bar: bartender, jukebox, counter interactions, and
   casino door. **Done** for this slice's scope (see Present Slice above);
   no scene ABI yet, per step 1.
4. Rebuild the casino as its own scene, then slots with proper queued reel
   animation. **Basic slot machine done** (bet/pull, staged queued reel
   animation, walked exit). Blackjack is a visual placeholder only, no game
   logic.
5. Rebuild the sewer/manhole route and add new destinations as separate
   banked scene modules. **First sewer chamber done** as an in-core scene;
   its collision/detail pass and banked-scene conversion remain pending.
6. Build All-Nite Mart and Moonlight Park as the first outward city scenes.
   **Mart exterior/interior and first clerk/register/shelf interactions are
   done**; build its inventory loop, then Moonlight Park after extracting a
   reusable scene layer. Then add Starlight Lodge, Galaxy
   Lanes & Arcade, Slug Acres, Pocket Zoo, and the Municipal Research Annex
   in the dependency order above.
7. Expand the built Piss Corner with a reusable banished-patron state and a
   clue/shortcut that responds to dialogue and puzzle progression.

## Scalability Systems

The live cartridge now uses fixed PRG banks 62-63 for the engine and separate
16 KiB PRG pairs for every current destination. New content should extend this
model rather than grow the old transitional runtime image.

1. **Scene registry and route table.** Expand the existing `V2SceneRoute`
   state into compact data records that identify a scene's PRG pair, CHR bank,
   default spawn, and music cue. Add exit-to-route records so a new doorway or
   death response is data plus a scene-local exit code, not another nested
   dispatcher branch.
2. **Item catalog.** Replace one-field-per-item state with compact item IDs,
   ownership bits, and read-only item metadata for names/descriptions/icons.
   Keep puzzle-critical items non-consumable by default.
3. **Hotspot/action records.** Give each scene small rectangle/action tables
   for Look, Talk, Take, and Use. Scene code should own rendering and movement;
   reusable fixed code should evaluate action prerequisites and apply results.
4. **Event and death results.** Standardize item grants, dialogue, sounds,
   flags, route changes, and comic deaths into a small result structure. Every
   death should select an explicit respawn `V2SceneRoute`.
5. **NPC profiles.** Move persistent dialogue progression and cross-scene NPC
   state into profiles, while each scene retains its own movement/render loop.
6. **Validation and diagnostics.** Extend the bank manifest with scene IDs,
   PRG/CHR ownership, and dependency checks. Add host tests for routes, item
   prerequisites, hotspot results, and checkpoint/death recovery. Add a
   compile-time debug overlay only after its fixed-bank budget is reserved.

### Capacity Rules

- The fixed engine currently has roughly 4.5 KiB free. It is for mapper/NMI,
  input, OAM/UI primitives, persistent state, shared rules, and the compact
  registries above only.
- Location-specific dialogue, NPC scripts, collision, art, music data, and
  death presentation belong in that location's PRG pair and CHR page.
- PRG pairs 15-61 and CHR banks 03-31 remain the expansion pool. Reserve a
  pair and a CHR bank in the manifest before adding a new destination.

## Rules For New Scenes

- Assign executable scene code only after its call enters through a fixed-bank
  trampoline; ordinary C code must not switch its own PRG window.
- Give each scene an explicit PRG/CHR allocation in `assets/banks/manifest.json`.
- Keep all gameplay VRAM updates on the NMI queue once the engine slice exists.
- Do not restore legacy room/collision code to the active build. Reuse behavior
  only after it has been reimplemented against the v2 scene contract.
- A `$2007` write that lands outside vblank/forced-blank doesn't fail
  quietly -- it corrupts the PPU's internal address and scatters garbage
  into unrelated tiles. This caused a real bug this slice (the casino's
  reel text). Any VRAM update triggered mid-gameplay (not at scene entry)
  must either force blank first or be proven to fit inside the remaining
  vblank budget; don't assume "right after `oam_flush()`" is automatically
  safe.
- Attribute color applies to a whole 4x4-tile (32x32px) block at once, never
  finer. Size and position any element that needs its own palette (a sign,
  a lit window, a UI panel) to exactly fill whole attribute blocks, or
  plain background sharing that block will pick up the wrong color too.
  Neon Avenue's sign and windows are the reference example of this.
