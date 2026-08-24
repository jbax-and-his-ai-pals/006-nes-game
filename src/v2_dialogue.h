#ifndef V2_DIALOGUE_H
#define V2_DIALOGUE_H

/* Draw, type, and dismiss the shared three-line dialogue overlay. Scenes set
 * their palette before calling it and redraw themselves after it closes. */
void show_message_box(const char *line1, const char *line2, const char *line3);

#endif
