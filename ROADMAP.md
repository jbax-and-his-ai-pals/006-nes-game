# Roadmap: Neon-Night Adventure

## New Direction

The finished exploration-combat prototype is archived under `archive/`. The
active project is an original, comedic graphic adventure for NES hardware:
small authored scenes, strong environmental storytelling, item puzzles, and
short comic conversations rather than real-time combat. Scenes read as
fixed-camera adventure tableaux: a side-on facade or landscape with an actor
walking through it, not a scrolling overworld or a dense top-down tile field.

The first `Leisure Suit Larry` provides only a useful venue rhythm: a bar,
casino, and late-night shop make a compact town feel like a complete night
out. Our setting, characters, names, art, dialogue, and puzzles are wholly
original.

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

## Interaction Model (as actually built)

An earlier draft of this document planned a cursor-and-verb-strip point-and-
click interface (Look/Use/Talk/Take, an icon inventory grid). That was never
built; what shipped instead, and is the real interaction model going forward:

- The player walks with the d-pad in every scene; there is no cursor.
- A pressed at a hotspot (an NPC, a machine, a door, a pickup) triggers that
  spot's one contextual action directly -- talk, enter a full-screen game,
  open a door, or collect an item. There is no separate verb selection step.
- START opens a text-list inventory (`src/v2_inventory.c`) of held items with
  short descriptions; there is no icon grid and no item-on-item/item-on-
  hotspot combination system yet (see Content Roadmap below).
- Dialogue is a typed, three-line message-box overlay (`src/v2_dialogue.c`),
  not a paged branching-choice panel.
- A location with its own real game (casino tables, the Commentarium's stage)
  gets its own full-screen presentation entered from the walkable room, not
  an in-room modal.

## Town Blueprint

| Location | Status | Scene role |
| --- | --- | --- |
| Neon Avenue | Built | Central hub; taxi death loop; gates every other surface destination. |
| Jimbles's Bar | Built | Tavern hub, jukebox/token puzzle, Piss Corner, casino and Commentarium doors. |
| Wrecked 'Em Casino | Built | Slots (wild BAR), blackjack (naturals), video poker (single-deck), roulette. |
| The Commentarium | Built | Stage performer and a rotating feed of comedic "dumb comments." |
| All-Nite Mart | Built | Convenience-store exterior/interior, shelf/cashier interactions, paper-clip pickup. |
| Sewer Network | Built | Underground shortcut layer connecting the Avenue, Park, and Zoo. |
| Moonlight Park | Built | Dry fountain, wandering patrons, service-hatch route to the Zoo. |
| Pocket Zoo | Built | Slug infestation, Keeper Nibs, the Zoo Map, pond/reptile exhibits. |
| Moonbeam Chapel | Planned | Quiet chapel with an overworked officiant; a ceremonial prop and a social puzzle. |
| Slug Acres | Planned | Eccentric suburban slug farm; creature-care comedy and a specialty puzzle resource. |
| Starlight Lodge | Planned | Hotel lobby and upper corridor, reached south from the Bar/Casino; trade info, reach a restricted room. |
| Galaxy Lanes & Arcade | Planned | Public bowling/arcade venue; token economy, a pattern minigame, social clue hub. |
| Municipal Research Annex | Planned | Late-game laboratory; evidence/item combination and the town-mystery reveal. |

See `WORLD_MAP.md` for the geography these connect through (including the
sewer's routing) and the expansion order for the still-planned destinations.

## Content Roadmap

The engine questions this document used to track in detail -- PRG/CHR
banking, the fixed-engine/scene-module split, the NMI VRAM queue -- are done
and described in `CURRENT_ARCHITECTURE.md`, which is the authoritative
reference for how the runtime actually works today. What's still open is
content and design work:

### Foundation (done)

- [x] Archive the completed exploration-combat NES prototype and its roadmap.
- [x] Eight playable locations (see Town Blueprint) with authored collision,
  door transitions, and street traffic death traps.
- [x] A modal, text-list inventory tracking six items with no puzzle-critical
  item currently losable.
- [x] Casino sharing one credit balance across four working games.
- [x] Fixed route/spawn state for scene transitions (`ROUTING.md`).

### Puzzles and progression (open)

- [ ] Ship one puzzle spanning multiple scenes (an item found in one location
  needed in another), not just a single-room pickup.
- [ ] Support item-on-hotspot and item-on-item actions -- the inventory has
  no interaction verb yet beyond holding/inspecting.
- [ ] Add puzzle prerequisites, state transitions, and alternate hint text
  for at least one multi-step puzzle.
- [ ] Add one conversation with a choice that changes flavor text or a clue,
  without branching the critical path.
- [ ] Write data-driven smoke tests for puzzle progression and failure paths,
  matching the existing pure-C test coverage for casino/quest/world logic.

### Town expansion (open)

- [ ] Build the next destination from `WORLD_MAP.md`'s expansion order
  (Moonbeam Chapel is next, off Moonlight Park's north exit).
- [ ] Give every new destination one signature mechanic: observation, social
  exchange, ritual, performance, or inventory comedy -- not a reskin of an
  existing minigame.
- [ ] Playtest the Zoo and Commentarium end to end from a clean boot with the
  same scrutiny already applied to the six earlier locations.

### Narrative and presentation (open)

- [ ] Write a one-page premise: protagonist, immediate objective, town
  secret, and ending condition. Still undecided.
- [ ] Write the complete critical path, optional jokes, and a hint ladder.
- [ ] Rework the title portrait into an intentional host/narrator, or decide
  it doesn't need one.
- [ ] Add scene-specific music loops and interaction sounds beyond what
  currently exists.
- [ ] Add transitions, cutaways, and a concise ending sequence.

### Shipping discipline (open)

- [ ] Add a fresh-save completion checklist and a no-dead-end regression
  pass once a critical path exists to regress against.
- [ ] Verify all text fits the NES font/dialogue constraints across every
  scene, not just the ones under active development.
- [ ] Package ROM, controls, attribution, and playtest notes.

## Immediate Next Steps

1. Decide the one-page premise (protagonist, objective, town secret, ending)
   -- most open content work depends on knowing what the critical path is.
2. Playtest Zoo and Commentarium from a clean boot; fix anything that only
   shows up in real play, the same way earlier locations were hardened.
3. Build Moonbeam Chapel (`WORLD_MAP.md`'s next expansion step) as the first
   destination designed against a real, if still one-page, premise.
4. Once a premise exists, design the first puzzle that genuinely spans two
   or more scenes, and add the item-on-hotspot interaction it needs.
