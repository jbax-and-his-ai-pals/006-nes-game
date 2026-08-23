/* quest.h -- hardware-independent fetch-quest assignment rules. */
#ifndef QUEST_H
#define QUEST_H
#include "rng.h"
typedef enum { QUEST_FETCH, QUEST_TALK } QuestVerb;
#define QUEST_ITEM_NONE 0xFF
unsigned char quest_assign_fetch_items(const unsigned char *verbs, unsigned char npc_count,
                                       unsigned char item_count, unsigned char *item_ids,
                                       unsigned char *scratch_pool, RngState *rng);
#endif
