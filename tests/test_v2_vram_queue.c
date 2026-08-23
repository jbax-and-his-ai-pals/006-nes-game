#include <stdio.h>

#include "../src/v2_vram_queue.h"

static int failures = 0;

static void check_eq(const char *name, int actual, int expected)
{
    if (actual != expected) {
        printf("FAIL %s: got %d, expected %d\n", name, actual, expected);
        ++failures;
    } else {
        printf("PASS %s\n", name);
    }
}

int main(void)
{
    unsigned char i;

    vram_queue_count = 0;
    check_eq("queues first tile", vram_queue_tile(13, 4, 99), 1);
    check_eq("first queue count", vram_queue_count, 1);
    check_eq("nametable address high byte", vram_queue_hi[0], 0x21);
    check_eq("nametable address low byte", vram_queue_lo[0], 0xA4);
    check_eq("queued tile data", vram_queue_tile_data[0], 99);

    vram_queue_count = 0;
    check_eq("queues arbitrary PPU address", vram_queue_address(0x23E1, 0x55), 1);
    check_eq("attribute address high byte", vram_queue_hi[0], 0x23);
    check_eq("attribute address low byte", vram_queue_lo[0], 0xE1);
    check_eq("attribute byte data", vram_queue_tile_data[0], 0x55);

    vram_queue_count = 0;
    check_eq("starts a private batch", vram_queue_batch_begin(), 1);
    check_eq("adds first private batch tile", vram_queue_batch_tile(16, 4, 8), 1);
    check_eq("adds private batch attribute", vram_queue_batch_address(0x23E1, 0x55), 1);
    check_eq("batch is hidden from NMI before commit", vram_queue_count, 0);
    check_eq("commits complete batch", vram_queue_batch_commit(), 1);
    check_eq("batch publish count", vram_queue_count, 2);
    check_eq("batch retains final byte", vram_queue_tile_data[1], 0x55);

    vram_queue_count = 1;
    for (i = 1; i < VRAM_QUEUE_CAPACITY; ++i) {
        (void)vram_queue_tile(0, i, i);
    }
    check_eq("queue reaches fixed capacity", vram_queue_count, VRAM_QUEUE_CAPACITY);
    check_eq("queue rejects overflow", vram_queue_tile(1, 0, 1), 0);
    check_eq("overflow preserves count", vram_queue_count, VRAM_QUEUE_CAPACITY);

    if (failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
