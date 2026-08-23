/*
 * test_read_pad.c -- unit test for the bit-assembly algorithm in
 * src/main.c's read_pad(), run under cc65's sim65.
 *
 * read_pad() itself touches real NES hardware ($4016) and can't run
 * outside an NES target, so this mirrors its algorithm against a mocked
 * shift register instead of compiling the real function. Keep this in
 * sync by hand if that algorithm ever changes.
 */

#include <stdio.h>

static unsigned char shift_bits[8];
static unsigned char shift_index;

static unsigned char mock_joypad_read(void)
{
    unsigned char v = shift_bits[shift_index];
    if (shift_index < 7) ++shift_index;
    return v;
}

static unsigned char read_pad(void)
{
    unsigned char result = 0;
    unsigned char i;

    shift_index = 0; /* strobe */

    for (i = 0; i < 8; ++i) {
        unsigned char bit = mock_joypad_read() & 0x01;
        result |= (unsigned char)(bit << i);
    }

    return result;
}

static int failures = 0;

static void check(const char *name,
                   unsigned char a, unsigned char b, unsigned char sel, unsigned char st,
                   unsigned char up, unsigned char down, unsigned char left, unsigned char right,
                   unsigned char expected)
{
    unsigned char result;
    shift_bits[0] = a;    shift_bits[1] = b;    shift_bits[2] = sel;  shift_bits[3] = st;
    shift_bits[4] = up;   shift_bits[5] = down; shift_bits[6] = left; shift_bits[7] = right;

    result = read_pad();
    if (result != expected) {
        printf("FAIL %s: got 0x%02X, expected 0x%02X\n", name, result, expected);
        ++failures;
    } else {
        printf("PASS %s\n", name);
    }
}

int main(void)
{
    /* Expected values match nes.h's JOY_*_MASK constants. */
    check("nothing pressed", 0,0,0,0, 0,0,0,0, 0x00);
    check("A only",          1,0,0,0, 0,0,0,0, 0x01);
    check("B only",          0,1,0,0, 0,0,0,0, 0x02);
    check("Select only",     0,0,1,0, 0,0,0,0, 0x04);
    check("Start only",      0,0,0,1, 0,0,0,0, 0x08);
    check("Up only",         0,0,0,0, 1,0,0,0, 0x10);
    check("Down only",       0,0,0,0, 0,1,0,0, 0x20);
    check("Left only",       0,0,0,0, 0,0,1,0, 0x40);
    check("Right only",      0,0,0,0, 0,0,0,1, 0x80);
    check("A+Right",         1,0,0,0, 0,0,0,1, 0x81);
    check("all pressed",     1,1,1,1, 1,1,1,1, 0xFF);

    if (failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
