#include "quest.h"
unsigned char quest_assign_fetch_items(const unsigned char *verbs, unsigned char npc_count,
                                       unsigned char item_count, unsigned char *item_ids,
                                       unsigned char *scratch_pool, RngState *rng)
{
    unsigned char fetch_count = 0, pool_count = item_count, i, pick;
    for (i = 0; i < npc_count; ++i) if (verbs[i] == QUEST_FETCH) ++fetch_count;
    if (fetch_count > item_count) return 0;
    for (i = 0; i < item_count; ++i) scratch_pool[i] = i;
    for (i = 0; i < npc_count; ++i) {
        if (verbs[i] != QUEST_FETCH) continue;
        pick = rng_next(rng) % pool_count;
        item_ids[i] = scratch_pool[pick];
        scratch_pool[pick] = scratch_pool[--pool_count];
    }
    return 1;
}
