#ifndef V2_SCENE_SIDESTREET_H
#define V2_SCENE_SIDESTREET_H

#define SIDESTREET_EXIT_MART 0
#define SIDESTREET_EXIT_TRAFFIC_DEATH 1
#define SIDESTREET_EXIT_ARCADE 2

/* One screen west of the Mart: two thinner storefronts (Pawn Shop and
 * Laundromat, see ROADMAP.md's Town Blueprint and Parked Ideas) with a
 * walkable path between them leading north to Galaxy Lanes & Arcade --
 * see v2_scene_arcade.h. Neither shop has an interior yet; both doors show
 * a "not open yet" message. Always arrives at the east edge, footpath-
 * from-the-Mart style; the west edge is a dead end for now. */
unsigned char run_sidestreet_exterior(void);

#endif
