#ifndef V2_INPUT_H
#define V2_INPUT_H

#define DEBOUNCE_RELEASE_FRAMES 4

unsigned char debounce_press(unsigned char pressed, unsigned char *held,
                             unsigned char *release_run);

#endif
