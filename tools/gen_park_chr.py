"""Build the Moonlight Park CHR page: the shared common bank with the
sewer-key graphic at tile 255 (the slot the common bank otherwise spends on
the jukebox token).

The park draws bar/neon/manhole/font tiles at the *common* indices, so its
page must be byte-identical to the common bank everywhere except tile 255.
Byte-patching the fixed engine's own core CHR guarantees exactly that: no
tile ordering can drift if a new .s joins the common build, because this
page is derived from the fixed engine, not re-assembled.

The key bytes mirror src/sewer_key.s (white bow/shaft/teeth over a solid
field) so the pickup reads as the same item it always has; on the park's
palette-1 foliage the field becomes dark green, so a white key sits bright
against the treetop."""

from pathlib import Path
import sys

HEADER = 16
PRG_SIZE = 2 * 8 * 1024      # banks 62-63, C000-FFFF
CORE_CHR_SIZE = 8 * 1024
KEY_TILE = 255

# Two bit-planes of an 8x8 tile, matching src/sewer_key.s.
KEY_PLANE0 = bytes([0x3C, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x1C, 0x18])
KEY_PLANE1 = bytes([0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF])
KEY_TILE_BYTES = KEY_PLANE0 + KEY_PLANE1


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
        print("usage: gen_park_chr.py FIXED_ENGINE.NES OUTPUT.BIN")
        return 2
    fixed = Path(sys.argv[1])
    out = Path(sys.argv[2])
    try:
        common = bytearray(read_common_chr(fixed))
        if len(KEY_TILE_BYTES) != 16:
            raise ValueError("key tile must be exactly 16 bytes")
        start = KEY_TILE * 16
        common[start : start + 16] = KEY_TILE_BYTES
    except (OSError, ValueError) as exc:
        print(f"park CHR error: {exc}")
        return 1
    out.write_bytes(common)
    print(f"Park CHR bank OK: {out} (common bank + key at tile {KEY_TILE})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
