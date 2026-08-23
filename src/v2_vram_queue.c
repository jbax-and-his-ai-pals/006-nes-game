#include "v2_vram_queue.h"

volatile unsigned char vram_queue_count;
unsigned char vram_queue_hi[VRAM_QUEUE_CAPACITY];
unsigned char vram_queue_lo[VRAM_QUEUE_CAPACITY];
unsigned char vram_queue_tile_data[VRAM_QUEUE_CAPACITY];
static unsigned char vram_queue_batch_count;
static unsigned char vram_queue_batch_active;

unsigned char vram_queue_address(unsigned int address, unsigned char value)
{
    unsigned char slot = vram_queue_count;

    if (vram_queue_batch_active || slot >= VRAM_QUEUE_CAPACITY) return 0;

    /* Publish the count last. C queues between frames, while the NMI claims
     * the published count before reading its corresponding entries. */
    vram_queue_hi[slot] = (unsigned char)(address >> 8);
    vram_queue_lo[slot] = (unsigned char)address;
    vram_queue_tile_data[slot] = value;
    vram_queue_count = (unsigned char)(slot + 1);
    return 1;
}

unsigned char vram_queue_batch_begin(void)
{
    if (vram_queue_batch_active || vram_queue_count) return 0;
    vram_queue_batch_count = 0;
    vram_queue_batch_active = 1;
    return 1;
}

unsigned char vram_queue_batch_address(unsigned int address, unsigned char value)
{
    unsigned char slot = vram_queue_batch_count;

    if (!vram_queue_batch_active || slot >= VRAM_QUEUE_CAPACITY) return 0;
    vram_queue_hi[slot] = (unsigned char)(address >> 8);
    vram_queue_lo[slot] = (unsigned char)address;
    vram_queue_tile_data[slot] = value;
    vram_queue_batch_count = (unsigned char)(slot + 1);
    return 1;
}

unsigned char vram_queue_batch_tile(unsigned char row, unsigned char col,
                                    unsigned char tile)
{
    return vram_queue_batch_address((unsigned int)0x2000 + (unsigned int)row * 32 + col,
                                    tile);
}

unsigned char vram_queue_batch_commit(void)
{
    if (!vram_queue_batch_active) return 0;
    /* This single publish is the only NMI-visible operation in the batch. */
    vram_queue_count = vram_queue_batch_count;
    vram_queue_batch_active = 0;
    return 1;
}

unsigned char vram_queue_tile(unsigned char row, unsigned char col,
                              unsigned char tile)
{
    return vram_queue_address((unsigned int)0x2000 + (unsigned int)row * 32 + col, tile);
}
