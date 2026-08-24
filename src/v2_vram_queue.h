#ifndef V2_VRAM_QUEUE_H
#define V2_VRAM_QUEUE_H

/* One queued tile costs three bytes: PPU address high/low, then tile ID.
 * The fixed 32-entry budget is intentionally small enough for one NMI. */
#define VRAM_QUEUE_CAPACITY 32

extern volatile unsigned char vram_queue_count;
extern unsigned char vram_queue_hi[VRAM_QUEUE_CAPACITY];
extern unsigned char vram_queue_lo[VRAM_QUEUE_CAPACITY];
extern unsigned char vram_queue_tile_data[VRAM_QUEUE_CAPACITY];

unsigned char vram_queue_tile(unsigned char row, unsigned char col,
                              unsigned char tile);

/* Queues one arbitrary nametable or attribute-table byte. Panel builders use
 * this for palette attributes while ordinary HUD callers use vram_queue_tile. */
unsigned char vram_queue_address(unsigned int address, unsigned char value);

/* Build a multi-write transaction without publishing partial work to NMI.
 * Call begin, add tiles/addresses, then commit once the row is complete. */
unsigned char vram_queue_batch_begin(void);
unsigned char vram_queue_batch_tile(unsigned char row, unsigned char col,
                                    unsigned char tile);
unsigned char vram_queue_batch_address(unsigned int address, unsigned char value);
unsigned char vram_queue_batch_commit(void);

/* A full-screen renderer owns the nametable and must discard any deferred
 * scene HUD writes before it starts direct PPU work. */
void vram_queue_clear(void);

#endif
