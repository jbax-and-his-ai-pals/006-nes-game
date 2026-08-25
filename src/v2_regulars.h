#ifndef V2_REGULARS_H
#define V2_REGULARS_H

/* Shared compact patron actor used by the Bar and Moonlight Park. */
void reset_piss_corner_regulars(void);
void update_piss_corner_regulars(void);
void reset_park_regulars(void);
void update_park_regulars(void);
unsigned char park_regular_is_near(unsigned char regular, unsigned char x,
                                   unsigned char y);
void draw_regulars(void);
void draw_bar_patron(void);

#endif
