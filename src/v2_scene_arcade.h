#ifndef V2_SCENE_ARCADE_H
#define V2_SCENE_ARCADE_H

#define ARCADE_EXIT_SIDESTREET 0
#define ARCADE_EXIT_GYM 1

/* Exterior only for now -- Galaxy Lanes & Arcade's own interior (darts,
 * shooting gallery, bowling, skeeball, whack-a-mole, claw machine; see
 * ROADMAP.md's Town Blueprint and Economy and Rewards sections) is still
 * planned. The door currently shows a "not open yet" message instead of
 * transitioning anywhere. No traffic here -- this is an alley behind the
 * Pawn Shop and Laundromat, not a real street. Runs until the player walks
 * back down the road to the side street (one screen south) or east to the
 * Municipal Gym; the west edge is still a dead end. The east edge is only a
 * functional exit for now -- the facade still fills that edge with plain
 * brick, since the perspective road/gap art it needs is a planned follow-up
 * revision, not part of this pass. */
unsigned char run_arcade_exterior(void);

#endif
