"""Decode the FamiTone2 note stream from music/title.s for inspection.

FamiTone2 packs each channel as a compact stream of note events and a few
control commands. This extracts the (note, duration) pairs from the square
(voice 0) and noise (voice 3) tracks so they can be transcoded into the
game's own table format. It is a diagnostic dump, not a build step.
"""

import sys


def cells(line):
    """Split an assembly data line into the byte values."""
    body = line.split(";")[0].replace(".byte", " ", 1)
    tokens = []
    for tok in body.split(","):
        tok = tok.strip().lower().lstrip("$")
        if tok:
            try:
                tokens.append(int(tok, 16))
            except ValueError:
                pass
    return tokens


def read_channel(path, label):
    """Return the list of byte values for @label..the next label/word line."""
    lines = open(path, encoding="ascii").read().splitlines()
    start = None
    for i, line in enumerate(lines):
        if label in line:
            start = i
            break
    if start is None:
        raise ValueError(f"{label} not found")
    data = []
    for line in lines[start + 1:]:
        line = line.strip()
        if not line or line.startswith(";"):
            continue
        if line.startswith("@") or line.startswith(".word") or line.startswith(".") and "byte" not in line:
            break
        if line.startswith(".byte"):
            data.extend(cells(line))
    return data


def decode(path, label):
    data = read_channel(path, label)
    notes = []
    i = 0
    while i < len(data):
        b = data[i]
        if 0xF0 <= b <= 0xFF:
            # Control commands (skip their operands).
            if b == 0xFF:
                i += 3      # count byte + little-endian word
            elif b == 0xFD:
                i += 3      # little-endian word target
            elif b == 0xF9 or b == 0xFB:
                i += 2      # nop / instrument arg
            elif b == 0xEB:
                i += 1      # nop
            else:
                i += 1
            continue
        if 0x80 <= b <= 0xEF:
            d = data[i + 1] if i + 1 < len(data) else 0
            notes.append((b & 0x7F, d))
            i += 2
            continue
        # low byte outside a note pair -- treat as a rest marker
        notes.append(("rest", b))
        i += 1
    return notes


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "music/title.s"
    for label in ("@song0ch0loop", "@song0ch3loop"):
        try:
            notes = decode(path, label)
        except ValueError as e:
            print(f"{label}: {e}")
            continue
        print(f"== {label} ==")
        for n, d in notes:
            if n == "rest":
                print(f"  rest({d})")
            else:
                print(f"  note {n:3d}  dur {d:3d}")
        print()


if __name__ == "__main__":
    main()
