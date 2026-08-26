/* music.h -- entry points for the NMI-owned music and lightweight effects. */

#ifndef MUSIC_H
#define MUSIC_H

void music_start(void);
void music_select_song(unsigned char song);
void music_play_jukebox_fanfare(void);
void music_sfx_swing(void);
void music_sfx_pickup(void);
void music_sfx_dialogue(void);
void music_sfx_hit(void);
void music_sfx_hurt(void);
void music_sfx_door(void);
void music_sfx_confirm(void);
void music_sfx_cancel(void);
void music_sfx_bell(void);

#define MUSIC_SONG_AVENUE 0
#define MUSIC_SONG_BAR 1
#define MUSIC_SONG_MART 2
#define MUSIC_SONG_SEWER 3
#define MUSIC_SONG_PARK 4
#define MUSIC_SONG_CASINO 5
#define MUSIC_SONG_ZOO 6
#define MUSIC_SONG_TITLE 7

#endif
