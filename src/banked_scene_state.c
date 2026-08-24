/* Fixed RAM bridge for arguments consumed by a freshly mapped scene entry. */
#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

volatile unsigned char banked_scene_arg;
