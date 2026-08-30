#ifndef V2_SCENE_GYM_H
#define V2_SCENE_GYM_H

#define GYM_EXIT_ARCADE 0
#define GYM_EXIT_INTERIOR 1

/* Municipal Gym sits one screen east of Galaxy Lanes & Arcade. Exterior and
 * interior are built; the interior's coach hotspot is a placeholder for the
 * planned live 1-on-1 basketball challenge (see ROADMAP.md) -- it currently
 * just talks. spawn: 0 arriving from the arcade (west edge), 1 leaving the
 * gym (centered on its door). See ROUTING.md's spawn table. */
unsigned char run_gym_exterior(unsigned char spawn);
void run_gym_interior(void);

#endif
