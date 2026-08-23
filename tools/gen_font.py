# Generates ca65 CHR tile data for an 8x8, 5x7-in-8x8, monochrome font.
# Each glyph is 7 rows of a 5-char ".X" pattern (col0..col4), padded to
# 8 bits (cols 5-7 blank) plus one blank row at the bottom (row7) for
# vertical spacing between text lines. Output: plane0 = pattern bits,
# plane1 = all one. Combined with the palette this loads (see font.h's
# comment), that gives ink pixels color index 3 and blank pixels color
# index 2 -- deliberately avoiding index 0/1, since a background tile's
# index-0 pixels always show palette 0's backdrop color regardless of
# which of the 4 background palettes is attribute-selected for that
# tile (an NES PPU quirk: only palette 0 entry 0 is ever the real
# backdrop). Avoiding index 0 entirely is what lets a text box use its
# own colors independent of whatever the room's backdrop is.

glyphs = {}

def g(name, rows):
    assert len(rows) == 7, name
    for r in rows:
        assert len(r) == 5, (name, r)
    glyphs[name] = rows

g('A', [".XXX.","X...X","X...X","XXXXX","X...X","X...X","X...X"])
g('B', ["XXXX.","X...X","X...X","XXXX.","X...X","X...X","XXXX."])
g('C', [".XXXX","X....","X....","X....","X....","X....",".XXXX"])
g('D', ["XXXX.","X...X","X...X","X...X","X...X","X...X","XXXX."])
g('E', ["XXXXX","X....","X....","XXXX.","X....","X....","XXXXX"])
g('F', ["XXXXX","X....","X....","XXXX.","X....","X....","X...."])
g('G', [".XXXX","X....","X....","X.XXX","X...X","X...X",".XXXX"])
g('H', ["X...X","X...X","X...X","XXXXX","X...X","X...X","X...X"])
g('I', ["XXXXX","..X..","..X..","..X..","..X..","..X..","XXXXX"])
g('J', ["..XXX","...X.","...X.","...X.","...X.","X..X.",".XX.."])
g('K', ["X...X","X..X.","X.X..","XX...","X.X..","X..X.","X...X"])
g('L', ["X....","X....","X....","X....","X....","X....","XXXXX"])
g('M', ["X...X","XX.XX","X.X.X","X...X","X...X","X...X","X...X"])
g('N', ["X...X","XX..X","X.X.X","X..XX","X...X","X...X","X...X"])
g('O', [".XXX.","X...X","X...X","X...X","X...X","X...X",".XXX."])
g('P', ["XXXX.","X...X","X...X","XXXX.","X....","X....","X...."])
g('Q', [".XXX.","X...X","X...X","X...X","X.X.X","X..X.",".XX.X"])
g('R', ["XXXX.","X...X","X...X","XXXX.","X.X..","X..X.","X...X"])
g('S', [".XXXX","X....","X....",".XXX.","....X","....X","XXXX."])
g('T', ["XXXXX","..X..","..X..","..X..","..X..","..X..","..X.."])
g('U', ["X...X","X...X","X...X","X...X","X...X","X...X",".XXX."])
g('V', ["X...X","X...X","X...X","X...X","X...X",".X.X.","..X.."])
g('W', ["X...X","X...X","X...X","X.X.X","X.X.X","XX.XX","X...X"])
g('X', ["X...X","X...X",".X.X.","..X..",".X.X.","X...X","X...X"])
g('Y', ["X...X","X...X",".X.X.","..X..","..X..","..X..","..X.."])
g('Z', ["XXXXX","....X","...X.","..X..",".X...","X....","XXXXX"])

g('0', [".XXX.","X...X","X..XX","X.X.X","XX..X","X...X",".XXX."])
g('1', ["..X..",".XX..","..X..","..X..","..X..","..X..",".XXX."])
g('2', [".XXX.","X...X","....X","...X.","..X..",".X...","XXXXX"])
g('3', [".XXX.","X...X","....X","..XX.","....X","X...X",".XXX."])
g('4', ["...X.","..XX.",".X.X.","X..X.","XXXXX","...X.","...X."])
g('5', ["XXXXX","X....","XXXX.","....X","....X","X...X",".XXX."])
g('6', [".XXX.","X....","X....","XXXX.","X...X","X...X",".XXX."])
g('7', ["XXXXX","....X","...X.","..X..","..X..","..X..","..X.."])
g('8', [".XXX.","X...X","X...X",".XXX.","X...X","X...X",".XXX."])
g('9', [".XXX.","X...X","X...X",".XXXX","....X","....X",".XXX."])

g('SPACE', [".....",".....",".....",".....",".....",".....","....."])
g('PERIOD',[".....",".....",".....",".....",".....",".XX..",".XX.."])
g('COMMA', [".....",".....",".....",".....",".....",".XX..",".X..."])
g('APOS',  [".XX..",".XX..",".X...",".....",".....",".....","....."])
g('EXCL',  ["..X..","..X..","..X..","..X..","..X..",".....","..X.."])
g('QUEST', [".XXX.","X...X","....X","...X.","..X..",".....","..X.."])
g('COLON', [".....",".XX..",".XX..",".....",".XX..",".XX..","....."])

order = ['SPACE','EXCL','APOS','COMMA','PERIOD','COLON','QUEST',
         '0','1','2','3','4','5','6','7','8','9',
         'A','B','C','D','E','F','G','H','I','J','K','L','M',
         'N','O','P','Q','R','S','T','U','V','W','X','Y','Z']

def row_byte(pattern):
    bits = ''.join('1' if c == 'X' else '0' for c in pattern) + '000'
    return bits

lines = []
lines.append("; ------------------------------------------------------------------------")
lines.append("; font.s -- 8x8 monochrome font tiles (generated; see tools/gen_font.py)")
lines.append(";")
lines.append("; One tile per glyph, in `order` below starting at FONT_TILE_BASE (see")
lines.append("; font.h). 5x7 pixels within the 8x8 cell (columns 5-7 and row 7 blank)")
lines.append("; for spacing when glyphs are placed edge-to-edge in the nametable.")
lines.append("; plane1 is all 1s, plane0 is the glyph shape -- ink pixels are color")
lines.append("; index 3, blank pixels are index 2 (see main.c's room_palettes")
lines.append("; comment for why index 0/1 are deliberately avoided).")
lines.append("; ------------------------------------------------------------------------")
lines.append("")
lines.append(".segment \"CHARS\"")
lines.append("")

for name in order:
    rows = glyphs[name]
    lines.append(f"; --- {name} ---")
    plane0 = ", ".join("%" + row_byte(r) for r in rows) + ", %00000000"
    lines.append(f".byte {plane0}")
    lines.append(".byte " + ", ".join(["$FF"] * 8))
    lines.append("")

with open("C:/jbax-and-his-ai-pals/006-nes-game/src/font.s", "w") as f:
    f.write("\n".join(lines) + "\n")

print("Wrote font.s with", len(order), "glyphs")
