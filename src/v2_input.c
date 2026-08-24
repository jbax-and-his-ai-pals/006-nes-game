#include "v2_input.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

unsigned char debounce_press(unsigned char pressed, unsigned char *held,
                             unsigned char *release_run)
{
    if (pressed) {
        *release_run = 0;
        if (!*held) {
            *held = 1;
            return 1;
        }
        return 0;
    }
    if (*release_run < DEBOUNCE_RELEASE_FRAMES) {
        ++*release_run;
    } else {
        *held = 0;
    }
    return 0;
}
