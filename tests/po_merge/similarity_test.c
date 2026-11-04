/* Similarity: 問い合わせのマスク、bit-parallel LCS、上限、比較値 */

#include <naos/naos.h>

#include "po_merge/merge_internal.h"

#include <stdint.h>

NAOS_TEST(similarity_scaffold_pattern_masks_are_64_bit_words)
{
    mnd_merge_pattern_t pattern;

    /* マスクの語は 64 ビットで、バイトの種類は 256 */
    NAOS_CHECK_UINT_EQ(sizeof(pattern.masks[0]), sizeof(uint64_t));
    NAOS_CHECK_UINT_EQ(MND_MERGE_PATTERN_WORD_BITS, 64);
    NAOS_CHECK_UINT_EQ(MND_MERGE_PATTERN_BYTE_KINDS, 256);
}

NAOS_SUITE(similarity,
    NAOS_ADD_TEST(similarity_scaffold_pattern_masks_are_64_bit_words)
)
