/* Hash: バイト列をキーとする開番地法のハッシュ表（FNV-1a、容量は 2 の累乗、使用率 1/2 以下） */

#include "po_merge/merge_internal.h"

#include <stdlib.h>

void mnd_merge_hash_init(mnd_merge_hash_t *hash)
{
    hash->slots = NULL;
    hash->cap = 0;
    hash->count = 0;
}

void mnd_merge_hash_free(mnd_merge_hash_t *hash)
{
    free(hash->slots);
    mnd_merge_hash_init(hash);
}
