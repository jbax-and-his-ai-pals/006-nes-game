"""Build the Municipal Gym's CHR page: the shared common bank plus a kit of
new tiles for the interior (a real 2x2 backboard/hoop, a solid court floor,
a half-court line and perimeter border, a mat, free weights, a bench) and
the exterior (corrugated metal siding and a roofline cap, breaking from the
brick-storefront facade every other building shares), at indices 70-83 --
the same general "item/enemy/critter span" the Arcade's marquee kit
already proved safe to patch (see tools/gen_arcade_chr.py), just starting
10 slots later so it doesn't collide with BAR_PATRON_BACK_TOP/BOTTOM
(59-60), which the Gym's own wandering NPCs need (see v2_scene_gym.c) even
though the Arcade's don't.

Tiles are authored as plain-text grids (one char per pixel, '0'-'3' picking
a palette index) and sliced into 8x8 CHR tiles by grid_to_tiles() -- much
easier to get right and to edit than hand-computing bitplane bytes. The
coach and the two wandering "gym rats" don't need any tiles here: they reuse
the game's existing 4-tile player-body sprite (PLAYER_TILE_BASE, see
v2_scene_gym.c's own wander logic) instead of new art, the same way Park's
and the Bar's own wandering NPCs do in their own scene files."""

from pathlib import Path
import sys

HEADER = 16
PRG_SIZE = 2 * 8 * 1024      # banks 62-63, C000-FFFF
CORE_CHR_SIZE = 8 * 1024

# Indices start at 70, not 60 -- 59-60 are BAR_PATRON_BACK_TOP/BOTTOM, the
# shared "facing away" sprite tiles the wandering-NPC system uses whenever
# an actor walks north (see v2_scene_gym.c's own regulars code). The Gym
# uses that same system for its two gym rats, so unlike the Arcade's
# marquee (which patches 60-64 safely -- nothing in the Arcade ever draws
# a wandering NPC), patching 60 here would show a chunk of backboard art
# instead of a body whenever a gym rat wandered north.
GYM_BACKBOARD_POST = 70
GYM_HOOP_TL = 71
GYM_HOOP_TR = 72
GYM_HOOP_BL = 73
GYM_HOOP_BR = 74
GYM_COURT_FLOOR = 75
GYM_MAT = 76
GYM_WEIGHTS_L = 77
GYM_WEIGHTS_R = 78
GYM_CENTER_LINE = 79
GYM_BORDER_H = 80
GYM_BENCH = 81
GYM_SIDING = 82
GYM_ROOF_CAP = 83
GYM_BALL = 84


def tile_from_rows(rows):
    """rows: 8 strings of 8 chars ('0'-'3', a palette index). Returns the
    16-byte CHR tile (plane0 + plane1)."""
    if len(rows) != 8 or any(len(row) != 8 for row in rows):
        raise ValueError(f"tile must be 8 rows of 8 chars, got {rows!r}")
    plane0 = bytearray(8)
    plane1 = bytearray(8)
    for r, row in enumerate(rows):
        b0 = b1 = 0
        for c, ch in enumerate(row):
            value = int(ch)
            bit = 7 - c
            if value & 1:
                b0 |= 1 << bit
            if value & 2:
                b1 |= 1 << bit
        plane0[r] = b0
        plane1[r] = b1
    return bytes(plane0) + bytes(plane1)


def grid_to_tiles(grid, tiles_wide, tiles_high):
    """grid: list of tiles_high*8 strings, each tiles_wide*8 chars wide.
    Returns the tiles in row-major order (top-left, top-right, ...)."""
    if len(grid) != tiles_high * 8:
        raise ValueError(f"grid must have {tiles_high * 8} rows, got {len(grid)}")
    tiles = []
    for ty in range(tiles_high):
        for tx in range(tiles_wide):
            rows = [grid[ty * 8 + ry][tx * 8 : tx * 8 + 8] for ry in range(8)]
            tiles.append(tile_from_rows(rows))
    return tiles


# Backboard: a thin vertical post (index 1 fill, index 3 edge trim), no
# horizontal seams -- stacking this tile vertically (for a post taller than
# one tile) reads as one continuous pole instead of a stack of separate
# panels. Mounted immediately outside each hoop (see show_gym_interior()),
# not as a wide panel above it, so the court reads as a long horizontal
# rectangle with a goal marker at each end rather than two boxy towers.
GYM_BACKBOARD_POST_BYTES = tile_from_rows([
    "31111113",
    "31111113",
    "31111113",
    "31111113",
    "31111113",
    "31111113",
    "31111113",
    "31111113",
])

# The 2x2 hoop: a wide rim bar (index 3) across the top, then a net (index 2)
# that stays a constant width -- not tapering to a point -- and ends in a
# blocky, crenellated bottom edge instead of a smooth cone.
GYM_HOOP_GRID = [
    "0003333333330000",  # rim top edge
    "0333333333333330",  # rim main bar
    "0022222222222200",  # net top band
    "0022222222222200",
    "0022222222222200",
    "0022222222222200",
    "0022222222222200",
    "0022222222222200",
    "0022222222222200",
    "0022222222222200",
    "0022222222222200",
    "0022002200220000",  # blocky bottom teeth
    "0022002200220000",
    "0000000000000000",
    "0000000000000000",
    "0000000000000000",
]
_hoop_tl, _hoop_tr, _hoop_bl, _hoop_br = grid_to_tiles(GYM_HOOP_GRID, 2, 2)
GYM_HOOP_TL_BYTES = _hoop_tl
GYM_HOOP_TR_BYTES = _hoop_tr
GYM_HOOP_BL_BYTES = _hoop_bl
GYM_HOOP_BR_BYTES = _hoop_br

# Court floor: a solid, flat fill -- no plank-seam texture. An earlier
# version had seams every 4 rows, but at this scale it read as a busy
# barcode of alternating stripes rather than a wood tone.
GYM_COURT_FLOOR_BYTES = tile_from_rows([
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
])

# A folded exercise mat: bordered (index 3) top/bottom with a center seam
# (index 2) splitting two fill bands (index 1).
GYM_MAT_BYTES = tile_from_rows([
    "33333333",
    "11111111",
    "11111111",
    "22222222",
    "11111111",
    "11111111",
    "11111111",
    "33333333",
])

# Free weights: two plates (index 2) joined by a short bar (index 3) between
# them, split into a left and right half-tile.
GYM_WEIGHTS_GRID = [
    "0022220000222200",
    "0222222002222220",
    "2222222222222222",
    "2222223333222222",
    "2222223333222222",
    "2222222222222222",
    "0222222002222220",
    "0022220000222200",
]
_weights_l, _weights_r = grid_to_tiles(GYM_WEIGHTS_GRID, 2, 1)
GYM_WEIGHTS_L_BYTES = _weights_l
GYM_WEIGHTS_R_BYTES = _weights_r

# Half-court line: a thin vertical stripe (index 3) down columns 3-4, same
# placement convention as the Arcade/side street's own rail-line tiles, so
# it reads as a court marking rather than a wall.
GYM_CENTER_LINE_BYTES = tile_from_rows([
    "00033000",
    "00033000",
    "00033000",
    "00033000",
    "00033000",
    "00033000",
    "00033000",
    "00033000",
])

# The court's perimeter border: a horizontal counterpart to the center
# line above, for the top and bottom edges (the line itself doubles as the
# left/right edges -- see show_gym_interior()).
GYM_BORDER_H_BYTES = tile_from_rows([
    "00000000",
    "00000000",
    "00000000",
    "33333333",
    "33333333",
    "00000000",
    "00000000",
    "00000000",
])

# A padded bench: bordered (index 3) top/bottom pad over a fill (index 1),
# open space, then two short metal legs (index 3) reaching the floor.
# Reused twice side by side (see show_gym_interior()) rather than authored
# as a left/right pair, the same trick the backboard post uses.
GYM_BENCH_BYTES = tile_from_rows([
    "03333330",
    "01111110",
    "01111110",
    "01111110",
    "03333330",
    "00000000",
    "00300300",
    "00300300",
])

# Exterior: corrugated metal siding, replacing the brick every other
# building shares -- a highlight ridge (index 3) every 3 rows over a plain
# fill (index 1), tileable both ways. Reused under two different palettes
# (see show_gym_exterior()) for a gold trim band near the roofline over a
# steel-blue body below, rather than one flat color top to bottom.
GYM_SIDING_BYTES = tile_from_rows([
    "33333333",
    "11111111",
    "11111111",
    "33333333",
    "11111111",
    "11111111",
    "33333333",
    "11111111",
])

# Exterior: a parapet lip for the roofline -- a bright cap (index 3) over a
# shadowed underside (index 2), reading as a lip overhanging the siding
# below instead of the wall just starting abruptly.
GYM_ROOF_CAP_BYTES = tile_from_rows([
    "33333333",
    "33333333",
    "33333333",
    "33333333",
    "22222222",
    "22222222",
    "22222222",
    "22222222",
])

# Exterior: a basketball icon (index 1 fill, index 3 equator seam) anchoring
# the name where a sign plate used to be -- an icon the name floats next to
# instead of a box the name sits inside, a different design language rather
# than another bordered-rectangle variant.
GYM_BALL_BYTES = tile_from_rows([
    "00111100",
    "01111110",
    "11111111",
    "33333333",
    "11111111",
    "11111111",
    "01111110",
    "00111100",
])


def read_common_chr(path: Path) -> bytes:
    """Return the fixed engine's core CHR (the common bank) as 8192 bytes."""
    image = path.read_bytes()
    expected = HEADER + PRG_SIZE + CORE_CHR_SIZE
    if len(image) != expected or image[:4] != b"NES\x1a":
        raise ValueError(f"{path}: not a {expected}-byte fixed-engine NES image")
    start = HEADER + PRG_SIZE
    return image[start : start + CORE_CHR_SIZE]


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: gen_gym_chr.py FIXED_ENGINE.NES OUTPUT.BIN")
        return 2
    fixed = Path(sys.argv[1])
    out = Path(sys.argv[2])
    patches = (
        (GYM_BACKBOARD_POST, GYM_BACKBOARD_POST_BYTES),
        (GYM_HOOP_TL, GYM_HOOP_TL_BYTES),
        (GYM_HOOP_TR, GYM_HOOP_TR_BYTES),
        (GYM_HOOP_BL, GYM_HOOP_BL_BYTES),
        (GYM_HOOP_BR, GYM_HOOP_BR_BYTES),
        (GYM_COURT_FLOOR, GYM_COURT_FLOOR_BYTES),
        (GYM_MAT, GYM_MAT_BYTES),
        (GYM_WEIGHTS_L, GYM_WEIGHTS_L_BYTES),
        (GYM_WEIGHTS_R, GYM_WEIGHTS_R_BYTES),
        (GYM_CENTER_LINE, GYM_CENTER_LINE_BYTES),
        (GYM_BORDER_H, GYM_BORDER_H_BYTES),
        (GYM_BENCH, GYM_BENCH_BYTES),
        (GYM_SIDING, GYM_SIDING_BYTES),
        (GYM_ROOF_CAP, GYM_ROOF_CAP_BYTES),
        (GYM_BALL, GYM_BALL_BYTES),
    )
    try:
        common = bytearray(read_common_chr(fixed))
        for tile_id, tile_bytes in patches:
            if len(tile_bytes) != 16:
                raise ValueError(f"tile {tile_id} must be exactly 16 bytes")
            start = tile_id * 16
            common[start : start + 16] = tile_bytes
    except (OSError, ValueError) as exc:
        print(f"gym CHR error: {exc}")
        return 1
    out.write_bytes(common)
    print(f"Gym CHR bank OK: {out} (common + backboard/hoop/court/equipment/"
          f"exterior at {GYM_BACKBOARD_POST}-{GYM_BALL})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
