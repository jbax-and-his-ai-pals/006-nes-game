# Roadmap

An NES action-RPG: a small, systemic village built from one-screen rooms
(Zelda-style traversal), populated by schedule-driven NPCs and a
procedural-but-grounded quest generator. Combat plays like Zelda but
resolves more like Morrowind — real-time swinging with a stat-driven
hit/miss underneath, rather than pure execution.

The guiding principle throughout: small, provable increments. Every phase
below should produce something you can drop into an emulator and see/do,
not just infrastructure.

Alongside that: hardware-independent logic (movement/collision now;
quest generation, NPC schedules, etc. later) lives in its own modules
with no `<nes.h>`/PPU dependency, and gets unit tests run under cc65's
headless `sim65` simulator (see `test.bat`). PPU/timing-dependent code
(rendering, OAM updates) can't be unit-tested that way and stays manually
verified in an emulator — see README.md's Testing section for why that
split exists and what it's already caught.

## Design pillars

- **A village, not a continent.** Roughly 40-80 rooms to start. Small
  enough to fully hand-place, large enough for the systemic layer to have
  room to work.
- **Quests are backed by real objects.** A generated quest references an
  actual NPC, item, and room — completing it flips real state (an NPC's
  disposition, a chest's contents, a door's lock), not just a string of
  flavor text.
- **NPCs have a life.** Home room, workplace room, a day/night schedule,
  and a standing want that seeds quests about them.
- **Progression is item-gating, not stat-gating.** A candle opens dark
  rooms, a key opens a door, a raft opens water — cheap in data, and it
  gives the quest generator natural reward targets.
- **Combat is Zelda verbs, Morrowind resolution.** See below.

## Combat design

Mechanically, combat looks like Zelda: real-time movement, directional
attack, immediate visual feedback, no menus. What's different is what
happens under the hood when an attack connects:

- **Hit resolution is stat-driven, not purely positional.** A visually
  clean swing can still miss, based on the player's skill with the
  weapon in hand versus the target's evasion. This is the single biggest
  lever for making combat *feel* like Morrowind rather than Zelda,
  despite using the same input verbs.
- **Weapon variety with real tradeoffs.** A small roster (dagger / sword
  / blunt, to start) with distinct speed, damage, and accuracy — not
  Zelda's linear "bigger sword is strictly better" progression.
  Different data, same swing animation and hitbox code.
- **A per-weapon-type skill byte that grows with use.** Landing hits with
  a weapon type slowly improves accuracy with it. Cheap (one byte per
  weapon type) and gives a legible sense of character growth that isn't
  just inventory upgrades.
- **A light fatigue/stamina resource.** Attacking costs a small,
  regenerating resource, so combat has pacing rather than being pure
  reflex-spam.
- **Enemies as small stat blocks, not just fixed patterns.** HP, evasion,
  damage on a shared enemy template means the same visual enemy can be
  reused at different effective difficulty (a rough analogue of
  Morrowind's leveled encounters) instead of needing unique art per
  difficulty tier — which also matters for ROM budget.

## World structure (recap)

- Each room = one full nametable (32x30 tiles). Room transitions swap
  between two fully-known static screens — no continuous-world streaming.
- The world is a small graph: each room has up to 4 neighbors plus
  special connections (doors, warps). A room's data is an ID plus a
  handful of metadata bytes (type, enemy set, item set, event flags).
- Only the current room's entities are ever live in RAM. Everything else
  collapses to compact per-room bitflags (cleared / item taken / NPC
  talked to).

## Quests & NPCs (recap)

- A quest is a small packed record: verb, giver, target (NPC/item/room),
  reward. Verbs to start: fetch, deliver, kill, find-and-talk.
- Dialogue and quest text are template-driven (Mad-Libs style), filled in
  with real object names/locations so it reads as specific even though
  it's assembled from a small template set.
- NPC schedules move them between home/workplace rooms by time of day.

## Progression & persistence

- Item-gated area access (candle, key, raft, ...).
- Battery-backed SRAM holds compact world state: quest flags, NPC
  disposition bytes, per-room one-time-event flags, player inventory and
  weapon skills. All small integers/bitflags — no free-form data.

## Phases

**Phase 0 — Toolchain & proof of concept.** ✅ Done. cc65 build pipeline,
Mesen emulator, a single moving sprite with correct vblank-safe input
handling.

**Phase 1 — One real room.** ✅ Done. Tile-based room rendering from the
nametable (walls + floor, not a bare sprite on black), a background
palette, and real per-axis collision against the room's walls, with
collision logic split into its own unit-tested module
(`src/movement.c`). Proves the rendering/collision foundation everything
else builds on.

**Phase 2 — A connected patch of rooms.** *(in progress)* Started as a
closed 2x2 loop of 4 rooms/biomes (meadow, desert, forest, snow),
walkable clockwise or counterclockwise; grew to 8 with a content pass
that hangs one dead-end room off each of those 4, each through a single
newly-opened exit:

```
             [Tundra]
                |
  [Swamp] -- [Forest] -- [Snow]
                |            |
             [Meadow] -- [Desert]
                |            |
             [Plains]     [Canyon]
```

Each room has exits *only* where it actually connects to something
(`RoomLayout`'s per-exit open/closed flags, plus a `RoomConnections`
table main.c uses to generalize the transition logic to all 4
directions), instead of every room having all 4 open. Every new room
reuses the existing tree/rock obstacle shapes recolored per biome, same
as the original 4 -- no new CHR data needed to give 4 more rooms their
own feel. Room switching is still an instant cut; a screen-slide
transition is still ahead. The new rooms also grew Phase 5's quest pool:
a third item (a crystal, in Tundra) and a fourth NPC (stationary, in
Swamp, `QUEST_FETCH`) -- `generate_quests()` now picks among 3 items for
3 fetch NPCs instead of 2-for-2, the first pool size where the shuffle
has more than one real outcome to prove.

A real lesson from building this: a large decoration list appeared to
crash the ROM (blank screen, reproduced reliably across many rebuilds).
Extensive investigation -- cycle-counting, a linker map audit, isolating
array size vs. arbitrary CPU load via an unrelated busy-loop, adding and
then removing speculative `waitvsync()` fixes -- eventually traced it to
Mesen itself: a freshly-launched emulator window can sit on a blank
screen for several real seconds before it starts painting actual frames,
with no error or indication anything is wrong. The ROM was correct the
entire time. See README.md's Testing section for the pitfall writeup.

**Phase 3 — Text and dialogue.** *(in progress)* A 43-glyph font (A-Z,
0-9, space, basic punctuation; `tools/gen_font.py` generates the CHR data
from readable ASCII-art glyph definitions) and a text box: press START to
show the current room's name centered in a bordered box across the
bottom of the screen, frozen movement while it's open, press START again
to close. `text.c` (nametable row fill/text-draw/attribute-row helpers)
and `font.c` (hardware-independent, tested char->glyph mapping) are
reusable building blocks -- what's not built yet is the actual
string/template engine (Mad-Libs-style quest/dialogue text) or anything
NPC-triggered; this proves the rendering pipeline works end to end first.

Notable NES quirk hit here: only palette 0 entry 0 is ever the real
backdrop color -- a background tile's index-0 pixels always show it
regardless of which of the 4 background palettes the attribute table
selects for that tile. The text box's font glyphs are encoded to use
color indices 2/3 instead of 0/1 specifically to get colors independent
of whatever the room's own backdrop is (see main.c's `room_palettes`
comment).

**Phase 4 — One hand-authored quest.** ✅ Done. Talk to an NPC, fetch an
item from another room, turn it in, see a flag flip and a reward
granted. Proves the quest-as-data pipeline before generalizing it.

First slice done: a single hand-placed NPC in the meadow, drawn as a
distinct standing-figure sprite (`src/npc.s`, its own CHR tile so its
index doesn't depend on renumbering the font/other tiles) in its own
sprite palette color, that blocks movement like a room obstacle and
opens a text-box line of dialogue when the player presses A while
adjacent. Movement/collision's `Obstacle`/`boxes_overlap()` (in
`movement.c`, unit-tested) generalize the existing tile-collision system
to a dynamic non-tile obstacle without any NPC-specific concept leaking
into that module. The text box also picked up a real frame
(`src/box_border.s`) instead of being a bare rectangle, and the
open/close button got debounced (`main()`'s `talk_release_frames`) after
holding it down turned out to retrigger rapidly under some input paths.

Second slice done: the full fetch quest. A one-tile item
(`src/item.s`, a small gem) sits in the forest; walking over it picks
it up (Zelda-style, no button) and sets `quest_state` to
`QUEST_ITEM_CARRIED`. Talking to the NPC while carrying it turns it in
(`quest_state` flips to `QUEST_COMPLETE`), the dialogue changes
(`npc_dialogue()`, now computed from quest state rather than a static
per-NPC string), and the NPC's sprite recolors to a "reward granted"
tell (`NPC_REWARD_PALETTE`) the player can see without opening the text
box. Still hardcoded/session-only, as expected this early: no quest-data
table yet (that's Phase 5's job) and no persistence across power cycles
(Phase 7's SRAM). Picking up the item also opens a "GOT A FLOWER!" text
box -- without it, a walk-over pickup with no inventory display yet to
check afterward was otherwise silent and easy to miss happened at all.

**Phase 5 — Quest generator + NPC schedules.** *(in progress)* Multiple
quest verb templates, NPCs that actually move between rooms by time of
day. This is where the systemic/Morrowind-flavored fantasy turns on.

First slice done: the NPC now has a `home`/`work` room pair instead of a
fixed room, swapping every `NPC_SCHEDULE_PERIOD` (300 frames, ~5s -- fast
enough to see move in a normal play/test session, not meant to represent
a real day yet) based on a frame counter (`game_ticks`). It doesn't just
teleport between them -- `update_npc_walk()` walks it in a straight line
(both rest spots sit on the same crossing-path column) to the room's
exit, across the room boundary, and to its rest spot in the new room,
same 1px/frame pace as the player, so the transition actually reads as
"the NPC walked to the other room" rather than a cut. Since the NPC can
now come and go while the player just stands in a room (no room reload
to hook a sprite update into), its OAM slot is refreshed every frame in
`main()`'s loop instead of once at room-entry, the same way the
player's already was.

A real NES lesson from this slice: after wiring the walk up, the NPC
stopped rendering *at all*, in any room, however its position or
schedule were computed -- confirmed with a fixed-position, always-drawn
debug sprite on the same OAM slot, which also refused to show up. The
cause was OAMADDR ($2003): every OAM write left it sitting at a nonzero
address, and since OAMADDR doubles as the PPU's own internal read
pointer during each scanline's sprite evaluation, leaving it dirty
across the vblank boundary corrupted sprite evaluation for the entire
next frame -- not just the sprite whose write was last. `set_oam_sprite()`
and `hide_oam_sprite()` now reset it to 0 before returning, unconditionally,
rather than trusting every call site to remember to.

Second slice done: two NPCs instead of one, each wanting its own item
and tracking its own quest progress (`quest_complete`), rather than the
single global `quest_state` Phase 4 shipped with. The meadow NPC keeps
its home/work schedule; a second, stationary NPC (home == work) sits in
the desert wanting a gem from the snow room -- proving the walk state
machine degrades cleanly to "never moves" without a separate code path,
and that the fetch-quest pattern (item, pickup message, three dialogue
strings, reward color) generalizes past a single hardcoded case.
`Item`/`Npc` are both plain data now (`items[]`/`npcs[]`), each entity
gets its own fixed OAM slot (`NPC_SLOT`/`ITEM_SLOT`) so any number of
NPCs/items can coexist without stepping on each other's sprite, and
`npc_dialogue()` takes an index instead of reading a single global.

Third slice done: a second quest verb. Every NPC so far had been
QUEST_FETCH (wants an item turned in); a third NPC, stationary in the
snow, is QUEST_TALK -- ready to complete the moment it's reached, no
item involved. The only place that distinction lives is
`npc_quest_ready()`; `npc_dialogue()` and the turn-in check in `main()`
call it without needing to know which verb they're dealing with. Matches
the fetch/deliver/kill/find-and-talk verb set this doc's Quests & NPCs
section already named -- QUEST_TALK is "find-and-talk".

Fourth slice done: the first actually-generated data in the project.
`src/rng.c` (a hardware-independent, unit-tested 8-bit Galois LFSR --
`tests/test_rng.c` checks it's maximal-length: all 255 non-zero states,
no short cycle) backs `generate_quests()`, called once at boot, which
assigns each QUEST_FETCH NPC an item to want from the pool of items no
other NPC has already claimed -- `npcs[]`' `item_id` fields are now
placeholders, not hand-picked. Dialogue for a fetch quest moved from the
NPC to the *item* (`request_message`/`turnin_message` on `Item`)
specifically so it stays correct regardless of which NPC generation
assigns it to. `rng_seed()` currently takes a fixed constant, not real
entropy -- documented in `rng.h` as a known, acceptable limit for proving
the assignment pipeline itself; a real seed source (something that
varies at boot) is separate, later work. Still a small, hand-placed pool
of NPCs/items for the generator to pick from -- growing that pool is
what makes the generation actually matter.

**Phase 6 — Combat + a small dungeon.** *(in progress)* The stat-driven
hit resolution, weapon variety, fatigue, and a multi-room dungeon built
on the same room-grid tech to give it somewhere to happen.

First slice done: real-time combat's input/feedback loop and the actual
"Morrowind resolution" hit-or-miss mechanic, proven with one hand-placed
enemy (a slime, in Canyon; `src/enemy.s` is a new sprite tile, distinct
from player/NPC/item). Movement now tracks `player_facing` (main.c has
no other notion of which way the player is pointed -- movement.c is
pure position math); pressing B swings in that direction, and
`roll_hit()` -- accuracy vs. evasion, clamped to a 5-95% range so no
matchup is ever a guaranteed hit or miss -- decides whether a swing that
connects actually lands. A clean-looking swing can still whiff, which is
the entire point of this slice: proving that resolution works and reads
correctly before weapon variety or a real enemy roster exist to combine
with it.

Second slice done, straight from playtest feedback on the first: the
slime wanders (`update_enemy_wander()` -- picks a direction every
30-90 frames or immediately after hitting a wall, no chase/flee AI
yet); touching it costs the player HP (`PLAYER_MAX_HP`, contact damage
with a brief invulnerability window so standing inside its box doesn't
drain HP every frame); the upper-left HUD displays that HP, and reaching
zero shows a brief faint message before respawning fully healed in Meadow;
and `Start` opens a pause menu with inventory and quit-to-title actions.
and a visible sword (`src/sword.s`) flashes at the swing point for a
few frames on every attack, hit or not. The original per-swing text box
turned out not to scale (it would freeze the frame on every single
attack) -- swing feedback is now non-blocking: a non-lethal hit makes
the enemy blink (`flash_timer`, toggled on/off by a bit of the
countdown rather than a second timer) for about a quarter second, a
miss is silent, and a killing blow just removes the sprite, no blink
needed since disappearing is already unambiguous. Weapon switching,
fatigue, an enemy roster, enemies attacking back, and dungeon room(s)
are all still ahead.

A second real NES lesson, discovered from this slice's own playtest
("I'm not seeing the sword when I press B"): manual per-sprite
$2003/$2004 writes don't just risk the stale-OAMADDR bug from Phase 5 --
their *cost scales with entity count*. Once a frame had to write player
+ up to 4 NPCs + the enemy + the sword, the later writes in that
sequence started landing after vblank ended and were silently dropped
(confirmed the same way as before: forcing a debug sprite write to the
very front of the frame worked, its normal spot after the NPC loop
didn't). Reordering would only have deferred the same bug to the next
entity added. Fixed properly this time: `main.c` now maintains a shadow
OAM buffer in RAM (`oam_buf`) that every sprite-setting function writes
to, and `oam_flush()` copies the whole thing to the PPU in one hardware
DMA transfer via `SPRITE_DMA` ($4014) -- a fixed 513-514 CPU cycles
every time, regardless of how many sprites are in use, comfortably
inside vblank's ~2273-cycle budget no matter how large the entity
roster grows. This also means individual sprite-update calls have no
timing deadline of their own any more (only the once-per-frame flush
does), which is what actually let the enemy's palette move from Canyon
to Meadow and the sword bug get fixed in the same pass -- confirmed
visually in Mesen: the slime renders reliably wandering around the
meadow, and a gold diagonal slash appears next to the player on every
swing.

**Phase 7 — Progression gating + save data.** An item unlocks new rooms;
battery-backed SRAM persists quest/NPC/room state and weapon skills
across sessions/power cycles.

**Phase 8 — Content pass.** Grow room count and NPC roster now that the
systems are proven; presentation polish (sound, title screen, etc.). The title
screen is now a midnight-road-sign composition: an enamel license plate for
`JERB'S GAME`, a deliberately crude four-color portrait (yellow glasses and
salt-and-pepper beard intact), and a smaller `NEW GAME` plate below. The title
portrait occasionally talks through a short, varied open/closed-mouth cycle.
It comes from a checked-in source image through a reproducible 10x12-tile CHR
conversion. The portrait now sits left of a right-side speech plate, cycling
through three deliberately taunting lines in sync with each talking burst.
A or START enters the existing game flow after release. Its looping
pulse-channel theme continues through gameplay.

## Open questions for later

- **Mapper choice.** We're on NROM (32KB PRG / 8KB CHR, no bank
  switching) for simplicity. Once room/content count grows past what
  NROM can hold, we'll need to move to a bank-switching mapper (MMC1 or
  MMC3 are the usual homebrew choices) — worth doing before content
  volume makes the migration painful, not after.
- **Sound.** Not scoped into any phase yet; slot in once the core loop
  (Phases 1-6) is proven fun.
