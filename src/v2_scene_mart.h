#ifndef V2_SCENE_MART_H
#define V2_SCENE_MART_H

#define MART_EXIT_AVENUE 0
#define MART_EXIT_INTERIOR 1
#define MART_EXIT_TRAFFIC_DEATH 2
#define MART_EXIT_SIDESTREET 3

/* spawn: 0 arriving from the avenue (east edge), 1 leaving the store
 * (centered on its door), 3 returning from the Pawn Shop/Laundromat side
 * street (west edge). See ROUTING.md's spawn table. */
unsigned char run_mart_exterior(unsigned char spawn);
void run_mart_interior(void);

#endif
