#include "font.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

unsigned char font_glyph_index(char c)
{
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');   /* fold lowercase -- one case only */
    }

    if (c >= 'A' && c <= 'Z') {
        return (unsigned char)(17 + (c - 'A'));
    }
    if (c >= '0' && c <= '9') {
        return (unsigned char)(7 + (c - '0'));
    }

    switch (c) {
        case '!':  return 1;
        case '\'': return 2;
        case ',':  return 3;
        case '.':  return 4;
        case ':':  return 5;
        case '?':  return 6;
        default:   return FONT_GLYPH_SPACE;
    }
}
