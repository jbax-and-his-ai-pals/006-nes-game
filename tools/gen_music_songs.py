"""Generate src/music_songs.s -- the central music bank.

Every song lives in its own fixed-stride block in one banked PRG pair, and
the fixed engine copies the current song's block into a fixed-RAM cache on
music_select_song(). This keeps all melody/bass/step data out of the fixed
engine (which is at its 2 KiB floor) and means songs never have to be moved
around as scenes grow.

Block layout (6 * STEP_SLOTS + 2 flags, all songs identical):
    melody_lo[STEP_SLOTS], melody_hi[...], bass_lo[...], bass_hi[...],
    step_frames[...], noise[...], has_bass(1), has_noise(1)

Song sources (see main()'s SOURCES table -- one line per song):
  * "table" songs (avenue, mart, sewer, park, casino... unless transcribed)
    are hand-composed 32-step period tables in src/scene_music_tables.s.
  * "txt" songs (the bar interior) come straight from a FamiStudio text
    export in music/ and are transcribed here.

The 2 A 0 3 register wiring (see music.s):
  * Square 1 (melody): period divider 16.  A $00 melody-low byte is a rest.
  * Triangle (bass):   period divider 32.  has_bass gates the voice.
  * Noise:             $0B = the game's crisp tick, $FF = rest.
  Both pitched voices write 0xF8 | (period>>8) as the high period byte, where
  the $F8 low bits are the APU length-counter load / constant bits.
"""

import re

TABLES = "src/scene_music_tables.s"   # hand-composed scene song tables
BAR_TXT = "music/bar_interior.txt"
TITLE_TXT = "music/title.txt"
OUT = "src/music_songs.s"

SONG_BLOCK_SIZE = 578              # 6 * STEP_SLOTS + two 1-byte flags
STEP_SLOTS = 96                    # every song reserves this many step slots
CPU = 1789772.5                   # NTSC

# Equal-temperament semitone offsets from A4 for each pitch class.
PITCH = {"c": -9, "c#": -8, "db": -8, "d": -7, "d#": -6, "eb": -6, "e": -5,
         "f": -4, "f#": -3, "gb": -3, "g": -2, "g#": -1, "ab": -1, "a": 0,
         "a#": 1, "bb": 1, "b": 2}

# Title tables, reproduced verbatim from the approved first transcription of
# music/title.txt (a silent triangle, crisp noise ticks, uniform 10-frame
# steps, and the 16-step melody repeated to fill the bank stride).
TITLE_TABLES = {
    "melody_lo": [0x3B, 0x3B, 0, 0, 0, 0xE3, 0, 0x5D, 0x5D, 0, 0, 0, 0,
                  0x5D, 0x3B, 0x5D] * 6,
    "melody_hi": [0xFA, 0xFA, 0xF8, 0xF8, 0xF8, 0xF9, 0xF8, 0xFA, 0xFA,
                  0xF8, 0xF8, 0xF8, 0xF8, 0xFA, 0xFA, 0xFA] * 6,
    "noise": [0x0B, 0xFF, 0xFF, 0x0B, 0x0B, 0x0B, 0xFF, 0x0B, 0xFF, 0x0B,
              0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF] * 6,
}


def note_to_period(name, divider, floor):
    """11-bit APU period for a FamiStudio note name like `G3`.

    A note below the `floor` Hz is raised octave by octave until it sits in a
    usable band (the title theme starts two octaves below the pulse's range,
    which is exactly why the original transcription moved it up).
    """
    m = re.fullmatch(r"([A-G])([#b]?)(-?\d+)", name.strip())
    if not m:
        raise ValueError(f"unrecognized note: {name!r}")
    letter, acc, octave = m.group(1), m.group(2).lower(), int(m.group(3))
    semi = PITCH[(letter + acc).lower()] + (octave - 4) * 12
    freq = 440.0 * (2.0 ** (semi / 12.0))
    while freq < floor:
        freq *= 2.0
    period = int(round(CPU / (divider * freq)))
    while period > 2047:
        period //= 2
    return period


def period_pair(name, divider, floor):
    p = note_to_period(name, divider, floor)
    return (p & 0xFF), (0xF8 | ((p >> 8) & 0x07))


def load_scene_tables(path):
    """Return {label: [32 bytes]} for every table in `path`."""
    tables = {}
    name = None
    vals = []
    with open(path, encoding="utf-8") as f:
        for raw in f:
            s = raw.strip()
            if not s or s.startswith(";"):
                continue
            if s.endswith(":") and not s.startswith("."):
                if name is not None:
                    tables[name] = vals
                    vals = []
                name = s[:-1]
            elif s.startswith(".byte"):
                for tok in s.replace(".byte", "", 1).split(","):
                    tok = tok.strip().lstrip("$")
                    if tok:
                        vals.append(int(tok, 16))
    if name is not None:
        tables[name] = vals
    for key, val in tables.items():
        if len(val) != 32:
            raise ValueError(f"{key} in {path}: {len(val)} bytes, expected 32")
    return tables


def parse_famistudio(path):
    """Return (channels, order).

    channels: {channel name: {pattern name: {step: (note name, duration)}}}
    order:     {channel name: [(instance order, pattern name), ...]}
    Steps are Time/10 -- the FamiStudio box-grid cell (NoteLength) maps to one
    engine step, matching how the title theme was transcribed.
    """
    channels = {}
    order = {}
    cur_ch = None
    cur_pat = None
    with open(path, encoding="utf-8") as f:
        for raw in f:
            s = raw.strip()
            if s.startswith('Channel Type='):
                cur_ch = s.split('"')[1]
                channels.setdefault(cur_ch, {})
                cur_pat = None
            elif s.startswith('Pattern Name='):
                cur_pat = s.split('"')[1]
                channels[cur_ch].setdefault(cur_pat, {})
            elif s.startswith('Note '):
                t = int(re.search(r'Time="(\d+)"', s).group(1))
                v = re.search(r'Value="([^"]+)"', s).group(1)
                d = int(re.search(r'Duration="(\d+)"', s).group(1))
                channels[cur_ch][cur_pat][t // 10] = (v, d)
            elif s.startswith('PatternInstance'):
                m = re.search(r'Time="(\d+)" Pattern="([^"]+)"', s)
                order.setdefault(cur_ch, []).append((int(m.group(1)),
                                                     m.group(2)))
    return channels, order


def channel_sequence(channels, patterns):
    """Flatten the given 16-step patterns into one note-name list (None =
    rest). Each pattern contributes 16 steps, in sequence."""
    out = []
    for idx, pname in enumerate(patterns):
        pat = channels.get(pname, {})
        for step in range(16):
            entry = pat.get(step)
            out.append(entry[0] if entry else None)
    return out


def transcribe_song(path, patterns, step_frame):
    """Build a song block from a FamiStudio export.

    Every FamiStudio note onset lands on the 10-tick grid, so each 16-step
    pattern is a 16-cell phrase at the SAME cell rate -- a pattern's note
    *duration* (5 vs 10) shapes articulation, not tempo. `step_frame` is the
    frames-per-cell for the whole song (a FamiStudio cell is one engine step).
    All three channels (Square 1 melody, Triangle bass, Noise) use the same
    pattern list.
    """
    channels, _order = parse_famistudio(path)
    melody = channels.get("Square1", {})
    bass = channels.get("Triangle", {})
    noise = channels.get("Noise", {})

    mseq = channel_sequence(melody, patterns)
    step_frames = [step_frame] * (len(patterns) * 16)

    melody_lo, melody_hi = [], []
    for n in mseq:
        lo, hi = period_pair(n, 16, 150.0) if n else (0, 0xF8)
        melody_lo.append(lo)
        melody_hi.append(hi)

    bass_lo, bass_hi = [], []
    for n in channel_sequence(bass, patterns):
        # The triangle is the bass voice; let it keep its composed register.
        lo, hi = period_pair(n, 32, 0.0) if n else (0, 0xF8)
        bass_lo.append(lo)
        bass_hi.append(hi)

    noise_arr = [0x0B if n else 0xFF for n in channel_sequence(noise, patterns)]
    has_bass = 1 if patterns else 0
    has_noise = 1 if patterns else 0
    return melody_lo, melody_hi, bass_lo, bass_hi, step_frames, noise_arr, \
        has_bass, has_noise


def pad_steps(data):
    """Resize a 32-step song table to the fixed bank stride by repeating it.
    Scenes and the title loop before the trailing slots, so the padding is
    never played; repeating keeps the block coherent if that ever changes."""
    data = list(data)
    while len(data) < STEP_SLOTS:
        data.extend(data)
    return data[:STEP_SLOTS]


def emit_block(out, melody_lo, melody_hi, bass_lo, bass_hi, step_frames,
               noise, has_bass, has_noise, label):
    out.append(f"{label}:")
    out.append(f'        ; {label} -- period bytes (melody/bass/step/noise).')
    for name, data in (("melody_lo", melody_lo), ("melody_hi", melody_hi),
                       ("bass_lo", bass_lo), ("bass_hi", bass_hi),
                       ("step_frames", step_frames), ("noise", noise)):
        if len(data) < STEP_SLOTS:
            raise ValueError(f"{label}_{name}: expected {STEP_SLOTS} bytes, "
                             f"got {len(data)}")
        out.append(f"{label}_" + name + ":")
        for i in range(0, STEP_SLOTS, 8):
            row = ",".join(f"${v:02X}" for v in data[i:i + 8])
            out.append(f"        .byte {row}")
    out.append(f"{label}_has_bass:")
    out.append(f"        .byte {has_bass}")
    out.append(f"{label}_has_noise:")
    out.append(f"        .byte {has_noise}")
    out.append("")


def table_block(tables, mlo, mhi, blo, bhi, sf):
    """Build a scene song block from hand-composed 32-step tables."""
    return (pad_steps(tables[mlo]), pad_steps(tables[mhi]),
            pad_steps(tables[blo]), pad_steps(tables[bhi]),
            pad_steps(tables[sf]), [0xFF] * STEP_SLOTS, 1, 0)


# One entry per song, in bank order. To add a song, append a row here:
#   ("song_foo", txt_block("music/foo.txt", ["Pattern 1", ...], step_frame))
# or point a table row at labels in src/scene_music_tables.s.
def main():
    tables = load_scene_tables(TABLES)

    songs = [
        ("song_avenue", table_block(tables, "melody_lo", "melody_hi",
                                    "bass_lo", "bass_hi", "step_frames")),
        # Bar interior: the engine loops its full 96-step arc (P1..P6).
        ("song_bar", transcribe_song(
            BAR_TXT, ["Pattern 1", "Pattern 2", "Pattern 3", "Pattern 4",
                      "Pattern 5", "Pattern 6"], step_frame=10)),
        ("song_mart", table_block(tables, "mart_melody_lo", "variant_melody_hi",
                                  "mart_bass_lo", "mart_bass_hi",
                                  "mart_step_frames")),
        ("song_sewer", table_block(tables, "sewer_melody_lo",
                                   "variant_melody_hi", "sewer_bass_lo",
                                   "sewer_bass_hi", "sewer_step_frames")),
        ("song_park", table_block(tables, "park_melody_lo", "variant_melody_hi",
                                  "park_bass_lo", "park_bass_hi",
                                  "park_step_frames")),
        ("song_casino", table_block(tables, "casino_melody_lo",
                                    "variant_melody_hi", "casino_bass_lo",
                                    "casino_bass_hi", "casino_step_frames")),
        ("song_zoo", table_block(tables, "zoo_melody_lo", "variant_melody_hi",
                                 "zoo_bass_lo", "zoo_bass_hi",
                                 "zoo_step_frames")),
    ]

    out = []
    out.append("; ------------------------------------------------------------------------")
    out.append("; music_songs.s -- the central music bank (generated by tools/gen_music_songs.py)")
    out.append(";")
    out.append("; All songs live here as fixed-stride 578-byte blocks. The fixed engine's")
    out.append("; music_select_song maps this bank, copies the chosen song's block into a")
    out.append("; fixed-RAM cache, and restores the scene bank. Entry 0 is the avenue theme;")
    out.append("; entry 1 is the bar interior (from music/bar_interior.txt). Every song")
    out.append("; reserves 96 step slots; scenes and the title use fewer and pad the rest.")
    out.append("; ------------------------------------------------------------------------")
    out.append("")
    out.append(".export music_song_ptrs")
    out.append('')
    out.append('.segment "CODE"')
    out.append("; Song pointer table at $8000 so the fixed engine can find song N by index.")
    out.append("music_song_ptrs:")
    for label, _block in songs:
        out.append(f"        .word {label}")
    out.append("        .word song_title")
    out.append("")
    out.append('.segment "RODATA"')

    for label, block in songs:
        emit_block(out, *block, label)

    # Title: embedded from the approved first transcription (a silent triangle,
    # crisp noise ticks, uniform 10-frame steps).
    emit_block(out, TITLE_TABLES["melody_lo"], TITLE_TABLES["melody_hi"],
               [0x00] * STEP_SLOTS, [0x00] * STEP_SLOTS, [10] * STEP_SLOTS,
               TITLE_TABLES["noise"], 0, 1, "song_title")

    open(OUT, "w", encoding="ascii").write("\n".join(out) + "\n")
    print(f"Wrote {OUT}: {len(songs) + 1} songs, {SONG_BLOCK_SIZE} bytes each")


if __name__ == "__main__":
    main()
