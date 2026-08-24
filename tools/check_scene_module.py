"""Validate the physical and linker-level contract of a banked scene image."""

from pathlib import Path
import re
import sys


CODE_START = re.compile(r"^CODE\s+([0-9A-Fa-f]{6})\s+([0-9A-Fa-f]{6})\s+([0-9A-Fa-f]{6})")


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: check_scene_module.py SCENE.BIN SCENE.MAP")
        return 2

    image = Path(sys.argv[1]).read_bytes()
    map_text = Path(sys.argv[2]).read_text(encoding="ascii")
    if len(image) != 0x4000:
        print(f"{sys.argv[1]}: expected a 16 KiB scene image, got {len(image)} bytes")
        return 1

    code_start = None
    for line in map_text.splitlines():
        match = CODE_START.match(line)
        if match:
            code_start = int(match.group(1), 16)
    # The entry object is linked first. Entries are either JSR target; RTS or
    # LDA #argument; JSR target; RTS. Checking bytes avoids depending on
    # linker-map presentation of an otherwise unreferenced assembly export.
    direct_entry = image[:1] == b"\x20" and image[3:4] == b"\x60"
    argument_entry = image[:1] == b"\xA9" and image[2:3] == b"\x20" and image[5:6] == b"\x60"
    ram_argument_entry = image[:1] == b"\xAD" and image[3:4] == b"\x20" and image[6:7] == b"\x60"
    multi_entry = (image[:1] == b"\xAD" and image[3:6] == b"\xC9\x02\xF0" and
                   image[7:8] == b"\x20" and image[10:11] == b"\x60" and
                   image[11:12] == b"\x20" and image[14:15] == b"\x60")
    if code_start != 0x8000 or not (direct_entry or argument_entry or ram_argument_entry or multi_entry):
        print(f"{sys.argv[2]}: scene entry must begin at $8000 and return after JSR")
        return 1

    print(f"Scene module OK: {sys.argv[1]} starts at $8000 and fills one PRG pair")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
