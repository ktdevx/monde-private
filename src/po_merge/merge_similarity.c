/* Similarity: 問い合わせのマスク、bit-parallel LCS、上限、比較値 */

#include "po_merge/merge_internal.h"

#include <stdlib.h>

void mnd_merge_pattern_free(mnd_merge_pattern_t *pattern)
{
    free(pattern->masks);
    free(pattern->vector);
    pattern->len = 0;
    pattern->words = 0;
    pattern->word_cap = 0;
    pattern->masks = NULL;
    pattern->vector = NULL;
}
