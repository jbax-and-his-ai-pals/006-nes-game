#ifndef V2_SCENE_MART_H
#define V2_SCENE_MART_H

#define MART_EXIT_AVENUE 0
#define MART_EXIT_INTERIOR 1
#define MART_EXIT_TRAFFIC_DEATH 2

unsigned char run_mart_exterior(unsigned char from_mart);
void run_mart_interior(void);

#endif
