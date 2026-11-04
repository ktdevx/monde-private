/* KeyIndex: msgctxt と msgid による索引と、重複の検出 */

#include "po_merge/merge_internal.h"

#include <stdlib.h>

void mnd_merge_key_index_free(mnd_merge_key_index_t *index)
{
    size_t i;

    mnd_merge_hash_free(&index->hash);
    if (index->keys != NULL)
    {
        for (i = 0; i < index->key_count; i++)
        {
            mnd_buf_free(&index->keys[i]);
        }
    }
    free(index->keys);
    index->keys = NULL;
    index->key_count = 0;
    index->catalog = NULL;
}
