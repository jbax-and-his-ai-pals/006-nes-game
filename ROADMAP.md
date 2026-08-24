# Roadmap: Neon-Night Adventure

## New Direction

The finished exploration-combat prototype is archived under `archive/`. The
active project is now an original, comedic graphic adventure for NES hardware:
small authored scenes, strong environmental storytelling, item puzzles, and
short comic conversations rather than real-time combat. Scenes should read as
fixed-camera adventure tableaux: a side-on facade or landscape with an actor
walking through it, not a scrolling overworld or a dense top-down tile field.

The first `Leisure Suit Larry` provides only a useful venue rhythm: a bar,
hotel-casino, chapel, dance club, and late-night shop make a compact town feel
like a complete night out. Our setting, characters, names, art, dialogue, and
puzzles will be wholly original. The intended player experience is closer to a
King's Quest-style adventure: observe, collect, combine, talk, and try odd
solutions.

## Experience Pillars

1. **One strange night in one walkable town.** Each destination has a distinct
   silhouette, color story, music cue, and reason to return.
2. **Readable comedy, never a dead end.** Failed actions produce a useful joke
   or clue. Essential items cannot be permanently lost.
3. **Small scenes with dense detail.** A focused 256x240 scene is better than a
   large empty map. Compose it like a stage set, with a readable ground plane,
   large silhouettes, and background details that invite inspection.
4. **Original characters and stakes.** The goal, protagonist, and every puzzle
   belong to this game, not to its inspirations.

## Town Blueprint

| Working destination | Scene role | Puzzle energy |
| --- | --- | --- |
| **Jimbles's Bar** | Starting tavern and rumor hub | Meet the bartender, find the first lead, learn the town's rules. |
| **Starlight Lodge & Lucky Wheel** | Hotel lobby, casino floor, upper corridor | Trade information, spot a rigged game, reach a restricted room. |
| **Moonbeam Chapel** | Quiet chapel with an overworked officiant | A ceremonial prop and a social puzzle, not a romance parody. |
| **The Comet Room** | Dance club and stage | Music, lighting, and a performance-based access puzzle. |
| **All-Nite Mart** | Convenience shop full of tiny usable objects | Inventory combinations, purchases, and comic inspections. |
| **Neon Avenue** | Exterior connector and taxi-stop view | Establish the night, gate destinations, and show progress visually. |
| **Piss Corner** | Lower-right nook inside Jimbles's Bar | Jimbles banishes rowdy regulars here; evolving barks and a later clue. |
| **Moonlight Park** | Small after-hours city park | Environmental observation, fountain/light puzzles, and meetings. |
| **Galaxy Lanes & Arcade** | Public bowling and arcade venue | Token economy, a pattern minigame, and a social clue hub. |
| **Slug Acres** | Eccentric suburban slug farm | Creature-care comedy and a specialty puzzle resource. |
| **Pocket Zoo** | Compact municipal zoo | Animal-behavior puzzle and keeper interactions. |
| **Municipal Research Annex** | Late-game laboratory | Evidence/item combination and the town-mystery reveal. |

## Interaction Model

- D-pad moves a cursor between hotspots; A selects.
- A compact verb strip offers **Look**, **Use**, **Talk**, and **Take** when
  applicable. B cancels or backs out.
- Inventory is an item grid, not a modal text list. Selecting an item then a
  hotspot attempts an item-specific action.
- A bottom dialogue panel supports short lines, choices, and action feedback.
- Every hotspot has a default `Look` response, including scenery.

## Milestones

### Phase 0: Preserve and Reframe

- [x] Archive the completed exploration-combat NES prototype and its roadmap.
- [ ] Replace the title-screen taunts with adventure-specific presentation.
- [ ] Write a one-page premise: protagonist, immediate objective, town secret,
  and ending condition.
- [ ] Decide whether the player avatar appears continuously in scenes or only
  in cutaway/action moments.

### Phase 0.5: Cartridge Architecture

- [x] Confirm the active build is mapper-4 MMC3: 512 KiB PRG-ROM plus 256 KiB
  CHR-ROM, with a 32 KiB CPU-visible runtime window awaiting scene banking.
- [ ] Split the common runtime (startup, NMI, controller, OAM, text, and
  bank-switch trampolines) from scene-specific code and data.
- [x] Add an MMC3 mapper-4 build target with 512 KiB PRG-ROM and 256 KiB
  CHR-ROM; keep the reset/vector bootstrap in the fixed bank.
- [x] Add a mapped-runtime budget guard and an active bank manifest; casino-owned
  data now occupies PRG bank 2 and the Park module occupies banks 3-4; banks
  5-61 remain available.
- [x] Establish a common CHR bank for font, HUD, player, and title art, then
  load scene-specific CHR banks on scene entry. Moonlight Park now selects
  CHR bank 2, initially as a stable common-graphics clone.
- [ ] Move each polished scene's art, dialogue, hotspots, and music data into
  named banks, with a small bank manifest and size budget report.

### Phase 1: Adventure Runtime

- [ ] Define `Scene`, `Hotspot`, `Item`, `Character`, and `Action` data types.
- [x] Replace the opening room's decoration-heavy tile interpretation with a
  direct, fixed-camera Neon Avenue tableau; scene art and collision are now
  free to evolve independently.
- [ ] Move opening-scene collision into explicit walkable bounds and hotspot
  rectangles, rather than inheriting the legacy top-down room grid.
- [ ] Build cursor movement, hotspot focus feedback, and the four-verb strip.
- [ ] Add a dialogue/action-result box that can render short paged text.
- [ ] Replace combat input with inspect/take/use/talk dispatch.
- [ ] Add deterministic game flags and a debug scene/item state display.
- [x] Add fixed route/spawn state for scene transitions; see `ROUTING.md`.
- [ ] Add a compact scene registry and data-driven exit routes before adding
  more than two new destinations.

### Phase 2: Inventory and Puzzle Safety

- [x] Implement a modal inventory list with selection and item descriptions;
  it currently tracks the paper clip, Jimbles's beer, jukebox token, sewer
  key, and temporary flamethrower pickup.
- [ ] Support item-on-hotspot and item-on-item actions.
- [ ] Add puzzle prerequisites, state transitions, and alternate hint text.
- [ ] Ensure no puzzle-critical item can be consumed, sold, or discarded into
  an unwinnable state.
- [ ] Write data-driven smoke tests for puzzle progression and failure paths.
- [ ] Replace per-item state fields with an item catalog and ownership bitset
  before the inventory grows beyond the current starter items.
- [ ] Add reusable hotspot/action and event-result records before building the
  first multi-scene puzzle.

### Phase 3: Vertical Slice

- [x] Build the first fixed-camera Neon Avenue pass: a night street, `BENT
  COIN` and `ALL NITE` facades, a taxi post, a welcoming resident, and no
  opening-room combat encounter.
- [x] Build the first Jimbles's Bar puzzle loop: collect a jukebox token,
  receive bartender guidance, activate the jukebox, and visibly unlock the
  back-room door. Activation plays Jimbles's one-shot theme, pauses, then
  resumes the normal loop; the door is currently a narrative hook, not a
  scene change.
- [x] Build Neon Avenue, Jimbles's Bar, and All-Nite Mart as connected,
  playable scenes with authored collision, door transitions, and street
  traffic death traps.
- [x] Expand the sewer into ladder shaft, flow junction, and dry-tunnel rooms;
  the tunnel contains a persistent sewer key with no puzzle dependency yet.
- [x] Turn Wrecked 'Em Casino into three working games sharing one credit
  balance: slots, blackjack, and video poker.
- [ ] Ship one three-step puzzle spanning all three scenes.
- [ ] Add one conversation with a choice that changes flavor text or a clue,
  without branching the critical path.
- [ ] Add scene-specific music loops and at least four interaction sounds.
- [ ] Playtest from a clean boot to the slice ending without developer tools.

### Phase 4: The Night Town

- [ ] Add Starlight Lodge & Lucky Wheel, Moonbeam Chapel, and The Comet Room.
- [x] Add Moonlight Park as an eastward Neon Avenue destination, with a dry
  fountain, paths, benches, and a distinct green night palette.
- [ ] Add Galaxy Lanes & Arcade, Slug Acres, Pocket Zoo, and the Municipal
  Research Annex in the dependency order in `REBUILD_ROADMAP.md`.
- [ ] Give every destination one signature mechanic: observation, social
  exchange, ritual, performance, or inventory comedy.
- [ ] Build a taxi/street transition map that reveals unlocked destinations.
- [ ] Author repeatable NPC barks that evolve with major story flags.

### Phase 5: Narrative and Presentation Pass

- [ ] Write the complete critical path, optional jokes, and a hint ladder.
- [ ] Replace placeholders with a cohesive neon-noir palette and scene art.
- [ ] Add transitions, cutaways, and a concise ending sequence.
- [ ] Rework the title portrait into an intentional host/narrator or remove it
  if it no longer serves the new tone.

### Phase 6: Shipping Discipline

- [ ] Add a fresh-save completion checklist and a no-dead-end regression pass.
- [ ] Verify all text fits the NES font/dialogue constraints.
- [ ] Verify scene changes, dialogue, music, and inventory updates remain
  vblank-safe.
- [ ] Package ROM, controls, attribution, and playtest notes.

## Immediate Next Steps

1. Playtest the new sewer exits, casino controls, and park transition in Mesen;
   tune collision halos and text only from observed behavior.
2. Continue extracting the handwritten scene loops from
   `src/v2_title_main.c` into the reusable scene/hotspot layer. Generic
   tile/text/attribute writes, dialogue, input debounce, and inventory now
   live in dedicated shared modules.
3. Add a second public venue, preferably Galaxy Lanes & Arcade, with one
   self-contained game and a non-critical collectible.
4. Make the inventory visual rather than text-only by assigning icons from a
   future scene-aware CHR bank, then design the first larger puzzle mechanic.
