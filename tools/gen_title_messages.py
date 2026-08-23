"""Generate fixed-width NES font-tile rows for title_messages.txt."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "title_messages.txt"
OUTPUT = ROOT / "src" / "title_messages.s"
MESSAGE_COUNT = 3
MESSAGE_WIDTH = 17

# Keep this mapping aligned with tools/gen_font.py's `order` and FONT_TILE_BASE.
FONT_TILE_BASE = 8
GLYPHS = " !',.:?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
TILES = {character: FONT_TILE_BASE + index for index, character in enumerate(GLYPHS)}


def load_messages():
    messages = [line.strip().upper() for line in SOURCE.read_text(encoding="ascii").splitlines()
                if line.strip() and not line.lstrip().startswith("#")]
    if len(messages) != MESSAGE_COUNT:
        raise SystemExit(f"Expected exactly {MESSAGE_COUNT} title messages, found {len(messages)}")
    for message in messages:
        if len(message) > MESSAGE_WIDTH:
            raise SystemExit(f"Message exceeds {MESSAGE_WIDTH} characters: {message!r}")
        unsupported = sorted(set(message) - set(TILES))
        if unsupported:
            raise SystemExit(f"Unsupported title-message character(s): {''.join(unsupported)!r}")
    return messages


messages = load_messages()
lines = [
    "; title_messages.s -- generated from assets/title_messages.txt.",
    "; Regenerate with: python tools/gen_title_messages.py",
    "",
    '.segment "RODATA"',
    ".export title_message_0, title_message_1, title_message_2",
    "",
]
for index, message in enumerate(messages):
    tiles = [TILES[character] for character in message.ljust(MESSAGE_WIDTH)]
    lines.append(f"; {message}")
    lines.append(f"title_message_{index}:")
    lines.append("        .byte " + ",".join(str(tile) for tile in tiles))
    lines.append("")

OUTPUT.write_text("\n".join(lines), encoding="ascii")
print(f"Wrote {OUTPUT}")
