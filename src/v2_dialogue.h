#ifndef V2_DIALOGUE_H
#define V2_DIALOGUE_H

/* Draw, type, and dismiss the shared three-line dialogue overlay. */
void show_message_box(const char *line1, const char *line2, const char *line3);
void dialogue_set_palette(unsigned char palette);
unsigned char dialogue_get_palette(void);

#endif
