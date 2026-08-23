# MMC3 Cartridge Layout

The active `game.nes` build is an iNES mapper-4 (MMC3) cartridge with:

- 512 KiB PRG-ROM (64 x 8 KiB banks)
- 256 KiB CHR-ROM (32 x 8 KiB banks)
- Horizontal mirroring and battery-backed RAM

`build.bat` currently links the v2 title, Neon Avenue, bar, and casino runtime as `game_core.nes`,
then `tools/pack_mmc3.py` places that core in MMC3's initial CPU window. The
legacy adventure source is no longer linked into `game.nes`:

| CPU window | MMC3 PRG bank | Core slice | Purpose |
| --- | ---: | --- | --- |
| `$8000-$9FFF` | 0 | first 8 KiB | switchable runtime window |
| `$A000-$BFFF` | 1 | second 8 KiB | switchable runtime window |
| `$C000-$DFFF` | 62 | third 8 KiB | MMC3 fixed penultimate bank |
| `$E000-$FFFF` | 63 | final 8 KiB | MMC3 fixed boot/vector bank |

The fixed `BOOT` segment at `$FFDE` runs after reset, enables the cartridge's
8 KiB PRG RAM, selects banks 0 and 1, then jumps to the existing cc65 startup
routine. Enabling PRG RAM is required because cc65 places mutable globals,
including title and music state, at `$6000`. This makes the normal runtime
visible exactly as it was under NROM while retaining MMC3's extra banks for
the adventure's future scene content.

Immediately after that jump, startup explicitly maps CHR registers 0-5 to
the first 8 KiB of common tile data. MMC3's power-on CHR register state is
not a rendering contract, so this initialization must happen before the
title screen writes any PPU tile indices. The title now explicitly selects
CHR bank 1 and gameplay restores CHR bank 0. Bank 1 is currently a
tile-compatible copy of the common bank, which makes this a safe first
runtime CHR-bank transition without changing title tile IDs.

## Bank Rules

- Bank 0/1 contain the current v2 compatibility-core runtime. Do not overwrite
  them with scene content.
- Banks 62/63 are fixed at `$C000-$FFFF`; reset and vectors rely on bank 63.
- Banks 2-61 are reserved for banked scene code/data.
- CHR bank 0 contains current common graphics. CHR bank 1 is the title bank;
  CHR banks 2-31 are reserved for scene-specific graphics.
- The next code-organization step is to establish the fixed-bank engine and
  its trampolines before assigning executable scene content to PRG banks 2-61.

## CHR Pattern-Table Halves

`mmc3_use_common_chr()`/`mmc3_use_title_chr()` map a CHR bank's full 8 KiB
(512 raw tile slots) into PPU `$0000-$1FFF`, but `PPU.control` is set once
at boot (`$80`) and never touched again, which pins *both* sprites and
backgrounds to reading pattern data from `$0000-$0FFF` only -- tile indices
0-255. The upper 4 KiB of every currently-used bank (indices 256-511) is
mapped and valid, just never selected; it is not a hardware limit, only an
unused register write. See `REBUILD_ROADMAP.md`'s "CHR Budget" section for
the concrete plan to use it (an independent background/sprite split via
`PPU.control`'s two pattern-table-select bits, plus real per-scene bank
switching using the reserved banks above).

`tools/pack_mmc3.py` is deliberately deterministic: it rejects a core ROM
with the wrong size or header before writing the mapper image.
