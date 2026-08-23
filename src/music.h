/* music.h -- entry points for the NMI-owned music and lightweight effects. */

#ifndef MUSIC_H
#define MUSIC_H

void music_start(void);
void music_play_jukebox_fanfare(void);
void music_sfx_swing(void);
void music_sfx_pickup(void);
void music_sfx_dialogue(void);
void music_sfx_hit(void);
void music_sfx_hurt(void);
void music_sfx_door(void);

#endif
