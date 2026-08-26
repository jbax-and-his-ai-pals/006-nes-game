#ifndef V2_SCENE_ARCADE_H
#define V2_SCENE_ARCADE_H

/* Exterior only for now -- Galaxy Lanes & Arcade's interior (darts,
 * shooting gallery, bowling, skeeball, whack-a-mole, claw machine; see
 * ROADMAP.md's Town Blueprint and Economy and Rewards sections) is still
 * planned. The door currently shows a "not open yet" message instead of
 * transitioning anywhere. No traffic here -- this is an alley behind the
 * Pawn Shop and Laundromat, not a real street. Runs until the player walks
 * back down the road to the side street, one screen south; the east and
 * west edges are both dead ends. */
void run_arcade_exterior(void);

#endif
