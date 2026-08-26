#ifndef V2_SCENE_CASINO_H
#define V2_SCENE_CASINO_H

/* Runs until the player walks through the casino's south exit (returns
 * CASINO_EXIT_OK), or loses every dollar (returns CASINO_EXIT_BROKE). */
#define CASINO_EXIT_OK 0
#define CASINO_EXIT_BROKE 1
unsigned char run_casino(void);

#endif
