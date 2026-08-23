#include <stdio.h>
#include "../src/quest.h"
static int failures = 0;
static RngState rng;
static void check_eq(const char *name, int actual, int expected) { if (actual != expected) { printf("FAIL %s: got %d, expected %d\n", name, actual, expected); ++failures; } else printf("PASS %s\n", name); }
int main(void)
{
    static const unsigned char verbs[] = {QUEST_FETCH, QUEST_TALK, QUEST_FETCH};
    static const unsigned char too_many[] = {QUEST_FETCH, QUEST_FETCH, QUEST_FETCH};
    unsigned char ids[] = {QUEST_ITEM_NONE, 77, QUEST_ITEM_NONE};
    unsigned char unchanged[] = {10, 11, 12};
    unsigned char pool[3];
    rng_seed(&rng, 42);
    check_eq("enough items assigns every fetch quest", quest_assign_fetch_items(verbs, 3, 3, ids, pool, &rng), 1);
    check_eq("first fetch gets a valid item", ids[0] < 3, 1);
    check_eq("talk assignment is untouched", ids[1], 77);
    check_eq("second fetch gets a valid item", ids[2] < 3, 1);
    check_eq("fetch quests receive distinct items", ids[0] != ids[2], 1);
    rng_seed(&rng, 42);
    check_eq("too few items reports failure", quest_assign_fetch_items(too_many, 3, 2, unchanged, pool, &rng), 0);
    check_eq("failure leaves assignments untouched", unchanged[0], 10);
    check_eq("failure leaves all assignments untouched", unchanged[2], 12);
    if (failures) return 1;
    printf("All checks passed\n");
    return 0;
}
