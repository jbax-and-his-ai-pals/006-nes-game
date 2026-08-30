"""Build the Galaxy Lanes & Arcade CHR page: the shared common bank with the
side street's thin alley road line re-patched at tile 2 (the arcade's road
uses it) plus a small set of NEW neon-marquee tiles at indices 60-64.

The arcade facade used to reuse the side street's page, so it inherited the
line tile but nothing neon of its own -- and its front was otherwise just the
bar's flat sign plate + big paired windows, recolored. These tiles let the
scene build a proper projected marquee (bright top lip, dark face, chasing
bulbs, lit under-lip) and small glowing panes, without spending any CHR the
rest of the game uses.

Indices 60-64 sit in the common bank's item/enemy/critter span, which this
facade never draws, so patching them here cannot break the arcade; the rest of
the page stays byte-identical to common (see build_arcade_chr.bat and
tools/gen_sidestreet_chr.py for the patch-from-common reasoning)."""

from pathlib import Path
import sys

HEADER = 16
PRG_SIZE = 2 * 8 * 1024      # banks 62-63, C000-FFFF
CORE_CHR_SIZE = 8 * 1024

ROAD_LINE_TILE = 2
MARQUEE_TUBE_TOP = 60
MARQUEE_PANEL = 61
MARQUEE_TUBE_UNDER = 62
MARQUEE_DOT = 63
NEON_LIT_WINDOW = 64

# Tile 2 -- the alley road's thin vertical rail: transparent (index 0) outside
# a 2px-wide line (index 1) down columns 3-4. Same art as the side street's.
ROAD_LINE_PLANE0 = bytes([0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18])
ROAD_LINE_PLANE1 = bytes([0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
ROAD_LINE_TILE_BYTES = ROAD_LINE_PLANE0 + ROAD_LINE_PLANE1

# Marquee top lip: a bright 2px neon line (index 3, white) across rows 0-1 on
# the marquee body (index 2, the bright purple of palette 1). Reads as the lit
# top edge of the projecting canopy.
MARQUEE_TUBE_TOP_PLANE0 = bytes([0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
MARQUEE_TUBE_TOP_PLANE1 = bytes([0xFF] * 8)
MARQUEE_TUBE_TOP_BYTES = MARQUEE_TUBE_TOP_PLANE0 + MARQUEE_TUBE_TOP_PLANE1

# Marquee face: a solid bright-purple (index 2) panel. The scene writes
# "ARCADE" over it with the font, whose white ink (index 3) pops on the purple.
MARQUEE_PANEL_PLANE0 = bytes([0x00] * 8)
MARQUEE_PANEL_PLANE1 = bytes([0xFF] * 8)
MARQUEE_PANEL_BYTES = MARQUEE_PANEL_PLANE0 + MARQUEE_PANEL_PLANE1

# Marquee under-lip: bright body above (index 2), a 2px neon line (index 3)
# across rows 6-7 -- the lit underside framing the door below.
MARQUEE_TUBE_UNDER_PLANE0 = bytes([0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF])
MARQUEE_TUBE_UNDER_PLANE1 = bytes([0xFF] * 8)
MARQUEE_TUBE_UNDER_BYTES = MARQUEE_TUBE_UNDER_PLANE0 + MARQUEE_TUBE_UNDER_PLANE1

# Chasing marquee bulb: a single bright dot (index 3) centered on the purple
# body (index 2), repeated along the canopy for a theater-marquee runway.
MARQUEE_DOT_PLANE0 = bytes([0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00])
MARQUEE_DOT_PLANE1 = bytes([0xFF] * 8)
MARQUEE_DOT_BYTES = MARQUEE_DOT_PLANE0 + MARQUEE_DOT_PLANE1

# Lit window pane: a bright border (index 3) around a glowing index-2 fill, on
# the dark wall. Small and self-contained so a pair flanks the arcade door
# without echoing the bar's big paired windows.
NEON_LIT_WINDOW_PLANE0 = bytes([0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF])
NEON_LIT_WINDOW_PLANE1 = bytes([0xFF] * 8)
NEON_LIT_WINDOW_BYTES = NEON_LIT_WINDOW_PLANE0 + NEON_LIT_WINDOW_PLANE1


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
        print("usage: gen_arcade_chr.py FIXED_ENGINE.NES OUTPUT.BIN")
        return 2
    fixed = Path(sys.argv[1])
    out = Path(sys.argv[2])
    patches = (
        (ROAD_LINE_TILE, ROAD_LINE_TILE_BYTES),
        (MARQUEE_TUBE_TOP, MARQUEE_TUBE_TOP_BYTES),
        (MARQUEE_PANEL, MARQUEE_PANEL_BYTES),
        (MARQUEE_TUBE_UNDER, MARQUEE_TUBE_UNDER_BYTES),
        (MARQUEE_DOT, MARQUEE_DOT_BYTES),
        (NEON_LIT_WINDOW, NEON_LIT_WINDOW_BYTES),
    )
    try:
        common = bytearray(read_common_chr(fixed))
        for tile_id, tile_bytes in patches:
            if len(tile_bytes) != 16:
                raise ValueError(f"tile {tile_id} must be exactly 16 bytes")
            start = tile_id * 16
            common[start : start + 16] = tile_bytes
    except (OSError, ValueError) as exc:
        print(f"arcade CHR error: {exc}")
        return 1
    out.write_bytes(common)
    print(f"Arcade CHR bank OK: {out} (common + road line at {ROAD_LINE_TILE}, "
          f"neon marquee at {MARQUEE_TUBE_TOP}-{NEON_LIT_WINDOW})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
