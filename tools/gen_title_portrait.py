"""Convert the generated reference portrait into a four-color NES CHR sheet."""

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "title_portrait_reference.png"
PREVIEW = ROOT / "assets" / "title_portrait_nes.png"
OUTPUT = ROOT / "src" / "title_portrait.s"
WIDTH = 80
HEIGHT = 96


def face_color(red, green, blue, y):
    """Map the detailed source into black, skin, glasses, and dark detail."""
    if max(red, green, blue) < 28:
        return 0
    if red > 180 and green > 165 and blue < 90:
        return 2
    if red > green + 22 and red > blue + 40 and red > 110:
        return 1
    if y >= 64 and (red + green + blue) // 3 > 125:
        return 2
    return 3


def chr_tile(pixels, left, top):
    values = []
    for plane in (0, 1):
        for row in range(8):
            value = 0
            for col in range(8):
                color = pixels[(left + col, top + row)]
                value |= ((color >> plane) & 1) << (7 - col)
            values.append(value)
    return values


source = Image.open(SOURCE).convert("RGB")
non_black = source.point(lambda value: 255 if value > 28 else 0).getbbox()
if non_black is None:
    raise SystemExit("Reference portrait has no visible pixels")

# The generated asset already has a black field; crop its visible silhouette.
portrait = source.crop(non_black).resize((WIDTH, HEIGHT), Image.Resampling.NEAREST)
pixels = {}
for y in range(HEIGHT):
    for x in range(WIDTH):
        pixels[(x, y)] = face_color(*portrait.getpixel((x, y)), y)

palette = [(0, 0, 0), (245, 163, 106), (255, 213, 28), (40, 58, 78)]

# The upper teeth use the lower band’s beard-white index. The tongue sits
# below this band and deliberately keeps the warmer skin/red index instead.
for y in range(63, 65):
    for x in range(40, 50):
        if (x, y) not in ((40, 63), (41, 63)) and pixels[(x, y)] == 1:
            pixels[(x, y)] = 2

preview = Image.new("RGB", (WIDTH, HEIGHT))
for position, color in pixels.items():
    preview.putpixel(position, palette[color])
preview.resize((WIDTH * 4, HEIGHT * 4), Image.Resampling.NEAREST).save(PREVIEW)

# A 3x3 patch reaches the mouth's upper edge as well as its open cavity. The
# closed version replaces that cavity with skin and a compact dark mouth line.
closed_pixels = dict(pixels)
for y in range(60, 78):
    for x in range(36, 52):
        # Stay within the open cavity rather than clearing the surrounding beard.
        if ((x - 44) * (x - 44) * 36 + (y - 69) * (y - 69) * 64) < 2600:
            if closed_pixels[(x, y)] in (0, 2, 3):
                closed_pixels[(x, y)] = 1
for x in range(37, 52):
    closed_pixels[(x, 65)] = 3
for x in range(39, 49):
    closed_pixels[(x, 66)] = 3

closed_preview = Image.new("RGB", (WIDTH, HEIGHT))
for position, color in closed_pixels.items():
    closed_preview.putpixel(position, palette[color])
closed_preview.resize((WIDTH * 4, HEIGHT * 4), Image.Resampling.NEAREST).save(
    ROOT / "assets" / "title_portrait_closed_nes.png"
)

lines = [
    "; title_portrait.s -- generated from assets/title_portrait_reference.png.",
    "; 10x12 background-tile portrait: 0=black, 1=skin, 2=yellow glasses, 3=beard/detail.",
    "; Regenerate with: python tools/gen_title_portrait.py",
    "",
    '.segment "CHARS"',
    "",
]
for tile_y in range(0, HEIGHT, 8):
    for tile_x in range(0, WIDTH, 8):
        values = chr_tile(pixels, tile_x, tile_y)
        lines.append(".byte " + ", ".join(f"${value:02X}" for value in values))

lines.extend(["", "; Closed-mouth replacement tiles: 3 columns by 3 rows."])
for tile_y in range(56, 80, 8):
    for tile_x in range(32, 56, 8):
        values = chr_tile(closed_pixels, tile_x, tile_y)
        lines.append(".byte " + ", ".join(f"${value:02X}" for value in values))

OUTPUT.write_text("\n".join(lines) + "\n", encoding="ascii")
print(f"Wrote {OUTPUT} and {PREVIEW}")
