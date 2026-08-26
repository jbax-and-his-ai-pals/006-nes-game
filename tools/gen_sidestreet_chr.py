"""Build the Pawn Shop/Laundromat side street's CHR page: the shared common
bank with two new line tiles patched in at tiles 1 and 2 (slots the common
bank otherwise spends on Jimbles's spare glass-bubble tile and the Piss
Corner regulars' facial overlay -- both irrelevant on this street).

The existing common-bank "line" tiles (box_border.s's border rules) fill
their non-line pixels with color index 2 instead of leaving them
transparent, because they were authored for message-box interiors where
index 2 is already black. On this street's own palette, index 2 is the
awning's pale yellow, so reusing them for the north path's rail lines would
paint a solid yellow bar instead of a thin line. These tiles use index 0
(the universal transparent backdrop) for everything except a 2px-wide
stripe in index 1, so they read as a thin line against whatever's behind
them regardless of palette -- see build_sidestreet_chr.bat and
src/v2_scene_sidestreet.c."""

from pathlib import Path
import sys

HEADER = 16
PRG_SIZE = 2 * 8 * 1024      # banks 62-63, C000-FFFF
CORE_CHR_SIZE = 8 * 1024
HLINE_TILE = 1
LINE_TILE = 2

# Two bit-planes of an 8x8 tile: a 2px-wide vertical line down columns 3-4,
# color index 1, transparent (index 0) everywhere else.
LINE_PLANE0 = bytes([0x18] * 8)
LINE_PLANE1 = bytes([0x00] * 8)
LINE_TILE_BYTES = LINE_PLANE0 + LINE_PLANE1

# Horizontal counterpart: a 2px-wide line across rows 3-4, same index 1 on
# transparent index 0 -- the "horizon" cap where the two rail lines meet.
HLINE_PLANE0 = bytes([0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00])
HLINE_PLANE1 = bytes([0x00] * 8)
HLINE_TILE_BYTES = HLINE_PLANE0 + HLINE_PLANE1


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
        print("usage: gen_sidestreet_chr.py FIXED_ENGINE.NES OUTPUT.BIN")
        return 2
    fixed = Path(sys.argv[1])
    out = Path(sys.argv[2])
    try:
        common = bytearray(read_common_chr(fixed))
        for tile_id, tile_bytes in ((LINE_TILE, LINE_TILE_BYTES), (HLINE_TILE, HLINE_TILE_BYTES)):
            if len(tile_bytes) != 16:
                raise ValueError("each patched tile must be exactly 16 bytes")
            start = tile_id * 16
            common[start : start + 16] = tile_bytes
    except (OSError, ValueError) as exc:
        print(f"sidestreet CHR error: {exc}")
        return 1
    out.write_bytes(common)
    print(f"Side Street CHR bank OK: {out} (common bank + lines at tiles {HLINE_TILE}/{LINE_TILE})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
