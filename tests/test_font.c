/*
 * test_font.c -- unit tests for src/font.c, run under cc65's sim65 (no
 * NES hardware, no emulator).
 *
 * Expected indices come from tools/gen_font.py's `order` list:
 * SPACE=0, EXCL=1, APOS=2, COMMA=3, PERIOD=4, COLON=5, QUEST=6,
 * '0'-'9'=7-16, 'A'-'Z'=17-42.
 */

#include <stdio.h>

#include "../src/font.h"

static int failures = 0;

static void check_eq(const char *name, int actual, int expected)
{
    if (actual != expected) {
        printf("FAIL %s: got %d, expected %d\n", name, actual, expected);
        ++failures;
    } else {
        printf("PASS %s\n", name);
    }
}

int main(void)
{
    check_eq("space",         font_glyph_index(' '),  0);
    check_eq("exclamation",   font_glyph_index('!'),  1);
    check_eq("apostrophe",    font_glyph_index('\''), 2);
    check_eq("comma",         font_glyph_index(','),  3);
    check_eq("period",        font_glyph_index('.'),  4);
    check_eq("colon",         font_glyph_index(':'),  5);
    check_eq("question mark", font_glyph_index('?'),  6);

    check_eq("digit 0", font_glyph_index('0'), 7);
    check_eq("digit 5", font_glyph_index('5'), 12);
    check_eq("digit 9", font_glyph_index('9'), 16);

    check_eq("letter A", font_glyph_index('A'), 17);
    check_eq("letter M", font_glyph_index('M'), 29);
    check_eq("letter Z", font_glyph_index('Z'), 42);

    check_eq("lowercase folds to uppercase: a", font_glyph_index('a'), font_glyph_index('A'));
    check_eq("lowercase folds to uppercase: z", font_glyph_index('z'), font_glyph_index('Z'));

    check_eq("unrecognized character falls back to space",
             font_glyph_index('@'), FONT_GLYPH_SPACE);

    check_eq("FONT_GLYPH_COUNT matches the glyph set size", FONT_GLYPH_COUNT, 43);

    if (failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
