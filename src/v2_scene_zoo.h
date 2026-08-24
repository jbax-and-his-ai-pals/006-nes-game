#ifndef V2_SCENE_ZOO_H
#define V2_SCENE_ZOO_H

#define ZOO_EXIT_SEWER 0
#define ZOO_EXIT_TRAFFIC_DEATH 1

/* The first Zoo module contains the service-room infestation and the locked
 * public frontage. It returns to the sewer until a surface shortcut exists. */
unsigned char run_zoo(void);

#endif
