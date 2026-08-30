"""Build All-Nite Mart's CHR page: the shared common bank plus a solid wall
panel (and a pair of mirrored seam variants of it, used as sparse edge
accent lines rather than a tiled grid), the same roofline-cap tile the Gym
uses (see tools/gen_gym_chr.py), and a genuinely solid glass tile with a
diagonal shine, at indices 60-65.
The window glass is its own tile rather than reusing the common bank's
NEON_TILE_WINDOW_FILL (211): that tile has a mullion hint baked in for use
in pairs (see draw_storefront_window() in v2_scene_ui.c), and repeating it
across a bigger area just repeats that hint into an unwanted grid -- a
proper "giant open window" needs a tile with no internal structure of its
own except the shine. The Mart doesn't have any wandering NPCs or anything
else using BAR_PATRON_BACK_TOP/BOTTOM (59-60), unlike the Gym, so unlike
the Gym's own page there's no need to start further into the "item/enemy/
critter span" to dodge a collision -- 60-62 is safe here the same way the
Arcade's marquee at 60-64 is.

Tiles are authored as plain-text grids (one char per pixel, '0'-'3'
picking a palette index) and sliced into 8x8 CHR tiles by grid_to_tiles()
-- much easier to get right and to edit than hand-computing bitplane
bytes."""

from pathlib import Path
import sys

HEADER = 16
PRG_SIZE = 2 * 8 * 1024      # banks 62-63, C000-FFFF
CORE_CHR_SIZE = 8 * 1024

MART_SIDING = 60
MART_ROOF_CAP = 61
MART_GLASS = 62
MART_SIDING_SEAM = 63
MART_SIDING_SEAM_H = 64
MART_SIDING_SEAM_L = 65
MART_DOOR_LINTEL = 66


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


# A smooth, solid panel -- no ridge texture at all, unlike GYM_SIDING (see
# tools/gen_gym_chr.py), for a flatter, glassier-looking wall than
# corrugated metal.
MART_SIDING_BYTES = tile_from_rows([
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
])

# The same solid panel with a single-pixel vertical seam (index 3) down its
# left edge. Placed every few tiles instead of every tile (see
# show_mart_exterior()), this reads as the joints between large flat
# panels -- a vertical, sparser, much subtler texture than the Gym's dense
# horizontal ridges, fitting a glassier building instead of corrugated
# metal.
MART_SIDING_SEAM_BYTES = tile_from_rows([
    "31111111",
    "31111111",
    "31111111",
    "31111111",
    "31111111",
    "31111111",
    "31111111",
    "31111111",
])

# The same solid panel with a horizontal seam along its top edge, the
# counterpart to MART_SIDING_SEAM above. Placed every 4 rows crossed with
# the vertical seam every 4 columns (see show_mart_exterior()), the two
# together form an actual grid of large panels -- real prefab building
# panels show seams both ways, not just one, which a single-direction
# stripe never reads as.
MART_SIDING_SEAM_H_BYTES = tile_from_rows([
    "33333333",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
])

# The mirror image of MART_SIDING_SEAM above, seam on the right edge
# instead of the left. Placed at the building's left edge (see
# show_mart_exterior()) so the accent line reads the same way on both
# flanks: a hair of green wall between the line and the open sky outside
# it, with the line itself sitting right next to the window frame inside
# it -- using the left-seam tile on both edges instead put the padding on
# opposite sides of the line at each flank, reading as lopsided.
MART_SIDING_SEAM_L_BYTES = tile_from_rows([
    "11111113",
    "11111113",
    "11111113",
    "11111113",
    "11111113",
    "11111113",
    "11111113",
    "11111113",
])

# A single-pixel white line over a wall-teal field -- deliberately its own
# tile rather than reusing TILE_BORDER_H (the window frame's border tile,
# common bank index 52): that tile is two-toned in most palettes (built for
# a frame that sits directly against glass), and standing alone above the
# door it read as an odd blue-and-green stripe instead of a clean line. A
# solid-white version of this same tile was tried first, but a full 8x8
# block of white read as a thick slab, not a thin line to match the
# vertical seam lines' own single-pixel width -- this keeps the same
# single-row thinness, just horizontal instead of vertical.
MART_DOOR_LINTEL_BYTES = tile_from_rows([
    "11111111",
    "11111111",
    "11111111",
    "33333333",
    "11111111",
    "11111111",
    "11111111",
    "11111111",
])

# A parapet lip for the roofline -- a bright cap (index 3) over a shadowed
# underside (index 2), the same shape as GYM_ROOF_CAP.
MART_ROOF_CAP_BYTES = tile_from_rows([
    "33333333",
    "33333333",
    "33333333",
    "33333333",
    "22222222",
    "22222222",
    "22222222",
    "22222222",
])

# Genuinely solid glass (index 2, matching the wall palette's existing
# blue) with a single-pixel diagonal shine (index 3, matching its existing
# white) running corner to corner. No mullion, no internal frame -- tiling
# this across a wide window doesn't repeat any hint of a divider, just the
# same diagonal streak, which lines up edge to edge into a continuous set
# of parallel shine lines across the whole glass area.
MART_GLASS_BYTES = tile_from_rows([
    "22222223",
    "22222232",
    "22222322",
    "22223222",
    "22232222",
    "22322222",
    "23222222",
    "32222222",
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
        print("usage: gen_mart_chr.py FIXED_ENGINE.NES OUTPUT.BIN")
        return 2
    fixed = Path(sys.argv[1])
    out = Path(sys.argv[2])
    patches = (
        (MART_SIDING, MART_SIDING_BYTES),
        (MART_ROOF_CAP, MART_ROOF_CAP_BYTES),
        (MART_GLASS, MART_GLASS_BYTES),
        (MART_SIDING_SEAM, MART_SIDING_SEAM_BYTES),
        (MART_SIDING_SEAM_H, MART_SIDING_SEAM_H_BYTES),
        (MART_SIDING_SEAM_L, MART_SIDING_SEAM_L_BYTES),
        (MART_DOOR_LINTEL, MART_DOOR_LINTEL_BYTES),
    )
    try:
        common = bytearray(read_common_chr(fixed))
        for tile_id, tile_bytes in patches:
            if len(tile_bytes) != 16:
                raise ValueError(f"tile {tile_id} must be exactly 16 bytes")
            start = tile_id * 16
            common[start : start + 16] = tile_bytes
    except (OSError, ValueError) as exc:
        print(f"mart CHR error: {exc}")
        return 1
    out.write_bytes(common)
    print(f"Mart CHR bank OK: {out} (common + solid wall panel/roof cap/glass/"
          f"seams/lintel at {MART_SIDING}-{MART_DOOR_LINTEL})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
