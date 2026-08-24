#ifndef V2_TRAFFIC_H
#define V2_TRAFFIC_H

/* Shared road hazard presentation. Scene loops own timing and respawn rules;
 * this service owns the taxi, debris OAM slots, and the common death plate. */
void draw_taxi_sprite(int x, unsigned char moving_left);
void hide_taxi_sprite(void);
void draw_traffic_particles(unsigned char x, unsigned char moving_left);
void draw_crash_tableau(unsigned char x, unsigned char moving_left);
void show_traffic_death(void);
void show_traffic_death_message(const char *line1, const char *line2);

#endif
