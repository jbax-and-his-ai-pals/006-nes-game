#ifndef V2_SCENE_BAR_H
#define V2_SCENE_BAR_H

#define BAR_EXIT_AVENUE 0
#define BAR_EXIT_CASINO 1
#define BAR_EXIT_COMMENTARIUM 2
/* Spilled or let the beer go warm: Jimbles banishes the player to Piss
 * Corner. The dispatcher re-enters the bar at the entrance, the same
 * respawn contract as the avenue's taxi death. */
#define BAR_EXIT_DEATH 3

unsigned char run_bar_interior(unsigned char from_casino);

#endif
