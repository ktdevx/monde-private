/* Fuzzy: 候補の登録、4-gram の索引、バイト長ごとの列、候補を調べる順序と最良の候補の選択 */

#include "po_merge/merge_internal.h"

#include <stdlib.h>

static void mnd_merge_index_list_free(mnd_merge_index_list_t *list)
{
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void mnd_merge_fuzzy_free(mnd_merge_fuzzy_t *fuzzy)
{
    size_t i;

    if (fuzzy->gram_lists != NULL)
    {
        for (i = 0; i < fuzzy->gram_list_count; i++)
        {
            mnd_merge_index_list_free(&fuzzy->gram_lists[i]);
        }
    }
    free(fuzzy->gram_lists);
    fuzzy->gram_lists = NULL;
    fuzzy->gram_list_count = 0;
    fuzzy->gram_list_cap = 0;
    for (i = 0; i <= MND_MERGE_FUZZY_SHORT_MAX_LEN; i++)
    {
        mnd_merge_index_list_free(&fuzzy->by_length[i]);
    }
    mnd_merge_hash_free(&fuzzy->grams);
    free(fuzzy->candidates);
    fuzzy->candidates = NULL;
    fuzzy->candidate_count = 0;
    free(fuzzy->counts);
    fuzzy->counts = NULL;
    free(fuzzy->touched);
    fuzzy->touched = NULL;
    fuzzy->touched_count = 0;
    mnd_merge_pattern_free(&fuzzy->pattern);
    fuzzy->catalog = NULL;
}
