# Banked Scene Contract

## Current Guardrail

The MMC3 cartridge has 64 PRG banks and 32 CHR banks, but the current
currently mapped runtime still occupies the CPU window at `$8000-$FFDD`. The build
now runs `tools/check_core_budget.py` and rejects a mapped runtime with fewer than 48
bytes of headroom. This is a safety margin, not a solution to scene scaling.

`assets/banks/manifest.json` is now active. It places the first casino-owned
data payload in PRG bank 2. `src/scenes/scene_park_entry.s` and
`src/scenes/scene_avenue_entry.s`, `src/scenes/scene_bar_entry.s`, and
`src/scenes/scene_mart_entry.s` link real 16 KiB modules in PRG banks 3-4,
5-6, 7-8, and 9-10 respectively. Avenue is callable from the fixed-layout
manual smoke route; the other scenes are independently linked and packed,
ready for banked route integration.
The build reports every banked asset in
`build/bank_report.txt`. The packer validates contiguous module bank lists
and rejects every overlapping assignment. Moonlight Park also selects CHR bank
2 at runtime; it currently contains a common-graphics clone so the scene keeps
its proven tile IDs while gaining an isolated art budget. PRG banks 11-61 and
CHR banks 3-31 remain available for future content.

`tests/test_pack_mmc3.py`, run by `test.bat`, verifies single-bank and paired
placement, common-CHR cloning, reserved runtime-bank protection, overlap
rejection, contiguity, asset kinds, and offsets. A malformed manifest now
fails during development rather than producing a subtly corrupted cartridge.

## Safe Migration Sequence

1. Move reset, NMI, music, mapper writes, VRAM queue, OAM, input, text, and
   game state into fixed banks 62/63 (`$C000-$FFFF`).
2. Define a fixed-bank scene trampoline that maps a contiguous PRG-bank pair
   into `$8000-$BFFF`, invokes `enter/update/render/leave`, then restores the
   prior pair before returning.
3. Link each scene module at `$8000-$BFFF` and assign it an exclusive 16 KiB
   pair from the available PRG banks. `SceneModule.prg_bank_first` identifies
   the first bank and the dispatcher maps that bank plus its immediate
   successor. Scene modules may call only fixed-engine APIs or code in their
   own pair.
4. Replace Park's initial common-tile CHR clone with authored scene art and
   give each additional scene its own CHR bank; update `PPU.control` so
   background and sprite pattern-table halves can use the full 512 tiles
   available in the mapped 8 KiB CHR bank.
5. Extend the manifest to reserve a PRG pair and CHR bank per scene, then make
   the build reject bank collisions and size overflow.

## Why No Far Call Yet

A trampoline in the current core would map itself and the NMI handler out of
memory when it selected a scene bank. That can corrupt an interrupt or return
to unmapped code. The fixed-engine extraction above is therefore a hard
prerequisite, not optional cleanup.
