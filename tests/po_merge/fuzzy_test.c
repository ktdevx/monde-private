/* Fuzzy: 候補の登録、4-gram の索引、候補を調べる順序と最良の候補の選択 */

#include <naos/naos.h>

#include "po_merge/merge_internal.h"

NAOS_TEST(fuzzy_scaffold_has_length_list_per_short_byte_length)
{
    mnd_merge_fuzzy_t fuzzy;

    /* 4-gram は 4 文字、バイト長 0〜28 の候補ごとに列を持つ */
    NAOS_CHECK_UINT_EQ(MND_MERGE_FUZZY_GRAM_CHARS, 4);
    NAOS_CHECK_UINT_EQ(MND_MERGE_FUZZY_SHORT_MAX_LEN, 28);
    NAOS_CHECK_UINT_EQ(sizeof(fuzzy.by_length) / sizeof(fuzzy.by_length[0]), 29);
}

NAOS_SUITE(fuzzy,
    NAOS_ADD_TEST(fuzzy_scaffold_has_length_list_per_short_byte_length)
)
