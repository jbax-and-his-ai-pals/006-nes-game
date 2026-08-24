#ifndef V2_SCENE_PARK_H
#define V2_SCENE_PARK_H

#define PARK_EXIT_AVENUE 0
#define PARK_EXIT_TRAFFIC_DEATH 1

/* Returns through the west edge or after a taxi death. */
unsigned char run_park(void);

#endif
