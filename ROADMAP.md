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

Underneath the fiction, this project is also a running exercise in how far
AI-assisted development can push a genuinely fixed, tiny target: a real MMC3
cartridge with hard per-bank budgets, not an abstraction of one. The town is
currently more a chain of self-contained vignettes -- walk somewhere, play
its game, walk to the next one -- than a plotted adventure with a critical
path, and that's fine as a mode to work in for a while. A one-page premise
(protagonist, objective, town secret, ending) is still real future work, see
Narrative and Presentation below, but it is not gating anything right now;
the current priority is breadth -- more systems, more locations, more
minigames -- explicitly drawing on inspiration from anywhere: other NES
games, arcade and boardwalk staples, and non-NES games old and new.

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
| Wrecked 'Em Casino | Built | Slots (wild BAR), blackjack (naturals), video poker (single-deck), roulette; cash-based, broke ends the night at the tables. |
| The Commentarium | Built | Stage performer and a rotating feed of comedic "dumb comments." |
| All-Nite Mart | Built | Convenience-store exterior/interior, shelf/cashier interactions, paper-clip pickup. |
| Sewer Network | Built | Underground shortcut layer connecting the Avenue, Park, and Zoo. |
| Moonlight Park | Built | Dry fountain, wandering patrons, service-hatch route to the Zoo. |
| Pocket Zoo | Built | Slug infestation, Keeper Nibs, the Zoo Map, pond/reptile exhibits. |
| Pawn Shop & Laundromat | In progress | Two exteriors sharing one screen, west of the Mart (`src/v2_scene_sidestreet.c`) -- both doors currently read "not open yet." Interiors still planned; the Pawn Shop's function (junk trinkets, see Economy and Rewards below) is decided, the Laundromat's is still just "deliberately mundane -- the 'why does this room even exist' joke room." The gap between them leads north to Galaxy Lanes & Arcade. |
| Galaxy Lanes & Arcade | In progress | Exterior built, reached north through the Pawn Shop/Laundromat side street (`src/v2_scene_arcade.c`) -- the door currently reads "not open yet." Interior still planned: darts, shooting gallery, bowling, skeeball, whack-a-mole, and the claw machine, all on the ticket economy (see Economy and Rewards below). |
| Casino Back Room | Planned | A second PRG pair reached by a door off the existing lobby -- the casino's own bank is exactly full (see `CURRENT_ARCHITECTURE.md`), so more casino games need fresh budget, not a squeeze. Houses three-card monte/shell game and high-low. |
| Moonbeam Chapel | Planned | Quiet chapel with an overworked officiant; a ceremonial prop and a social puzzle. |
| Slug Acres | Planned | Eccentric suburban slug farm; creature-care comedy and a specialty puzzle resource. |
| Starlight Lodge | Planned | Hotel lobby and upper corridor, reached south from the Bar/Casino; trade info, reach a restricted room. |
| Municipal Research Annex | Planned | Late-game laboratory; evidence/item combination and the town-mystery reveal. |
| The Diner | Planned | Lower-stakes hangout; hosts the hot dog eating contest. |
| Maze (hedge or corn, TBD) | Planned | A proper maze room; which flavor depends on which other location it ends up paired with. |
| Haunted House | Planned | Spooky-comic tone; likely a light reflex/reaction mechanic rather than pure atmosphere. |
| Used Car Lot | Planned | Plays off the taxi/traffic framing already established elsewhere. |
| Graveyard | Planned | Late-night, spooky-comic tone. |
| Barber Shop | Planned | Comedic QTE or a sprite customization screen; undecided which. |

See `WORLD_MAP.md` for the geography these connect through (including the
sewer's routing) and the expansion order for the still-planned destinations.
Where a new location connects to the existing map (which surface exit, which
neighbor) is not yet decided for anything below Municipal Research Annex in
this table -- that's `WORLD_MAP.md` work to do once a location moves from
planned to in-progress.

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
- [x] Casino sharing one cash balance across four working games, with a
  broke game-over state and its own respawn when it hits $0.
- [x] Real scene-specific music (all eight built locations) and a working
  chugging-challenge minigame gating the bar's jukebox token.
- [x] Pawn Shop & Laundromat exteriors, one screen west of the Mart
  (`src/v2_scene_sidestreet.c`) -- first proof that a new destination can
  be added by extending an existing scene's boundary (see
  `CURRENT_ARCHITECTURE.md` for the pattern to repeat), plus a scene
  sharing two storefronts on one screen.
- [x] Galaxy Lanes & Arcade's exterior, reached north through the side
  street's own gap (`src/v2_scene_arcade.c`) -- second proof of the same
  boundary-extension pattern, this time off a north/south edge instead of
  east/west.
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

### Economy and Rewards (open)

- [ ] Add a tickets currency, separate from cash: earned from the arcade's
  fair/skill games (darts, shooting gallery, bowling, skeeball, whack-a-
  mole), spent at a prize counter for items cash can't buy directly. Keeps
  the arcade's skill challenges from being trivialized by casino luck, and
  gives a broke player (cash at $0) something to do besides walk away.
- [ ] Add a persistent high-score/scoreboard system for at least one arcade
  game -- bowling is the natural first candidate, real bowling alleys
  already have the beat-the-high-score-for-a-prize trope built in. Beating
  the existing best score hands over a specific item, not just bragging
  rights.
- [ ] Claw machine: a pool of junk grabs plus one unique, harder-to-grab
  item worth $50 -- a second route to the flamethrower's existing $50
  price at the Mart (`src/v2_scene_mart.c`), not a new reward invented
  from scratch.
- [ ] Junk trinkets scattered around town (a gold tooth, a busted pocket
  watch, a creepy doll, ...) that exist only to be found and sold. Gives
  the Pawn Shop (exterior built, see Town Blueprint above) a real function
  without competing with any puzzle-critical item for inventory "importance."

### Town expansion (open)

- [ ] Pick the next destination to actually build. `WORLD_MAP.md`'s
  expansion order still names Moonbeam Chapel next, but the current
  priority is breadth over following that order strictly -- the Town
  Blueprint's newly-planned rows (Casino Back Room, The Diner, and the
  rest) are all live candidates too.
- [ ] Give every new destination one signature mechanic: observation, social
  exchange, ritual, performance, or inventory comedy -- not a reskin of an
  existing minigame. (The newly planned locations above already each have
  one identified; this still applies to whatever comes after them.)
- [ ] Playtest the Zoo and Commentarium end to end from a clean boot with the
  same scrutiny already applied to the six earlier locations.

### Narrative and presentation (open, not currently a priority)

- [ ] Write a one-page premise: protagonist, immediate objective, town
  secret, and ending condition. Still undecided, and deliberately not
  blocking other work right now -- see New Direction above.
- [ ] Write the complete critical path, optional jokes, and a hint ladder.
- [ ] Rework the title portrait into an intentional host/narrator, or decide
  it doesn't need one.
- [ ] Add interaction sounds for the new locations/minigames above as they
  get built; every existing location already has its own music track and a
  handful of sound effects (see Foundation above), this is about keeping
  pace with new content, not starting from nothing.
- [ ] Add transitions, cutaways, and a concise ending sequence.

### Shipping discipline (open)

- [ ] Add a fresh-save completion checklist and a no-dead-end regression
  pass once a critical path exists to regress against.
- [ ] Verify all text fits the NES font/dialogue constraints across every
  scene, not just the ones under active development.
- [ ] Package ROM, controls, attribution, and playtest notes.

## Parked Ideas

Raised during brainstorming, genuinely liked in some cases, but not
committed to a specific location or system yet. Kept here so nothing gets
lost; pull from this list freely when picking what to build next.

**Minigames:** insult contest (Monkey Island-style comeback duel),
bartending QTE, a Simon-says pattern game, a small shoot-em-up/maze-chase
arcade cabinet, ring toss, a test-your-strength high striker, pool/
billiards, a wheel of fortune, karaoke, a coin pusher, pinball, free-throw
hoops, arm wrestling.

**Locations:** a thrift/antique store, a radio station, a rooftop, a photo booth,
a fortune-teller/Zoltar machine, a drive-in movie theater, a taxi dispatch
office, a boardwalk/pier as something distinct from the indoor arcade, a
tattoo parlor alongside the barber shop.

**Systems:** an NPC reputation/buzz meter that shifts barks based on
wins/losses, a jukebox letting the player pick a location's music
on demand (relevant once more songs exist -- see New Direction above), a
day/night or weather atmosphere pass.

**Items:** the lucky charm (a casino-odds nudge) is adopted as a concept
but doesn't have a source yet now that the claw machine's rare item is
earmarked for flamethrower money instead -- still needs a home. Also
raised: sunglasses/a disguise for an ID-check interaction, a harmonica or
kazoo for a busking-for-cash beat.

## Immediate Next Steps

1. Give Galaxy Lanes & Arcade an interior -- it's the more fleshed-out of
   the two "in progress" rows in the Town Blueprint (its interior scope is
   already decided), so it's the natural next thing to finish rather than
   start something new. Pick one game to build first (darts is probably
   the simplest) and stand up the ticket currency and prize counter
   alongside it; the economy is easiest to get right when there's exactly
   one game to test it against.
2. Give the Pawn Shop an interior next -- the junk-trinket economy it
   needs (see Puzzles and progression above) makes it the second-most-
   decided "in progress" row. The Laundromat's own interior can wait until
   its premise firms up beyond "deliberately mundane."
3. Once the arcade has at least one real game, pick the next thing from
   the remaining Town Blueprint rows (Casino Back Room, The Diner, or any
   of the others) -- breadth is still the goal, not a fixed order.
4. Playtest Zoo and Commentarium from a clean boot; fix anything that only
   shows up in real play, the same way earlier locations were hardened.
5. Keep this roadmap in sync as things get built -- move rows from Planned
   to Built, and pull from Parked Ideas as slots open up.
