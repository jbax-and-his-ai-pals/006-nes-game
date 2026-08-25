#ifndef V2_PLAYER_ACTOR_H
#define V2_PLAYER_ACTOR_H

#define PLAYER_FACING_DOWN  0
#define PLAYER_FACING_UP    1
#define PLAYER_FACING_LEFT  2
#define PLAYER_FACING_RIGHT 3

void draw_player_sprite(unsigned char x, unsigned char y, unsigned char walking,
                        unsigned char facing);
void hide_player_sprite(void);

#endif
