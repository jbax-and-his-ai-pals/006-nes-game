#ifndef V2_RUNTIME_H
#define V2_RUNTIME_H

/* Shared fixed-core services. Scenes use these instead of touching the
 * controller latch or sprite-DMA page directly. */
#define PAD_A     0x01
#define PAD_B     0x02
#define PAD_SELECT 0x04
#define PAD_START 0x08
#define PAD_UP    0x10
#define PAD_DOWN  0x20
#define PAD_LEFT  0x40
#define PAD_RIGHT 0x80

extern unsigned char oam_buf[256];

unsigned char read_pad(void);
void clear_sprites(void);
void set_oam_sprite(unsigned char slot, unsigned char x, unsigned char y,
                    unsigned char tile, unsigned char attr);
void hide_oam_sprite(unsigned char slot);
void oam_flush(void);

#endif
