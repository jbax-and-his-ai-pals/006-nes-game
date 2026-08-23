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

- [x] Confirm the current cc65 `nes` target is fixed NROM: 32 KiB PRG-ROM
  plus 8 KiB CHR-ROM, and keep the active build inside that limit.
- [ ] Split the common runtime (startup, NMI, controller, OAM, text, and
  bank-switch trampolines) from scene-specific code and data.
- [x] Add an MMC3 mapper-4 build target with 512 KiB PRG-ROM and 256 KiB
  CHR-ROM; keep the reset/vector bootstrap in the fixed bank.
- [ ] Establish a common CHR bank for font, HUD, player, and title art, then
  load scene-specific CHR banks on scene entry.
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

### Phase 2: Inventory and Puzzle Safety

- [ ] Implement an inventory grid with item icons and descriptions.
- [ ] Support item-on-hotspot and item-on-item actions.
- [ ] Add puzzle prerequisites, state transitions, and alternate hint text.
- [ ] Ensure no puzzle-critical item can be consumed, sold, or discarded into
  an unwinnable state.
- [ ] Write data-driven smoke tests for puzzle progression and failure paths.

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
- [ ] Ship one three-step puzzle spanning all three scenes.
- [ ] Add one conversation with a choice that changes flavor text or a clue,
  without branching the critical path.
- [ ] Add scene-specific music loops and at least four interaction sounds.
- [ ] Playtest from a clean boot to the slice ending without developer tools.

### Phase 4: The Night Town

- [ ] Add Starlight Lodge & Lucky Wheel, Moonbeam Chapel, and The Comet Room.
- [ ] Add Moonlight Park, Galaxy Lanes & Arcade, Slug Acres, Pocket Zoo, and
  the Municipal Research Annex in the dependency order in `REBUILD_ROADMAP.md`.
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

1. Build the inventory menu around the existing paper clip, jukebox token,
   and beer state, then let one item solve a small recoverable puzzle.
2. Extract the handwritten scene loops from `v2_title_main.c` into a reusable
   scene/hotspot layer before adding a third full destination.
3. Polish the sewer's walkable space and build Moonlight Park as the first
   outward destination with its own visual identity and one simple puzzle.
4. Expand the casino only after the interaction layer is reusable: blackjack
   is still visual-only, while slots already provide the first minigame loop.
