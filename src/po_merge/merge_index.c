/* KeyIndex: msgctxt と msgid による索引と、重複の検出 */

#include "po_merge/merge_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * entry のキーの連結の長さ（1 + msgctxt + 1 + msgid）を *len に入れる。
 * msgctxt のないエントリでは msgctxt の内容を無視する。長さが size_t に収まらなければ false
 */
static bool key_length(const mnd_po_entry_t *entry, size_t *len)
{
    size_t context_len = 0;
    size_t n;

    if (entry->has_msgctxt)
    {
        context_len = entry->msgctxt.len;
    }

    if (context_len > (SIZE_MAX - 2))
    {
        return false;
    }

    n = context_len + 2;
    if (entry->msgid.len > (SIZE_MAX - n))
    {
        return false;
    }

    *len = n + entry->msgid.len;

    return true;
}

/* entry のキーの連結を dst（key_length の長さ以上）に書く */
static void key_write(unsigned char *dst, const mnd_po_entry_t *entry)
{
    size_t pos = 0;

    if (entry->has_msgctxt)
    {
        dst[pos] = (unsigned char)MND_MERGE_KEY_HAS_MSGCTXT;
        pos++;
        if (entry->msgctxt.len > 0)
        {
            memcpy(dst + pos, entry->msgctxt.data, entry->msgctxt.len);
            pos += entry->msgctxt.len;
        }
    }
    else
    {
        dst[pos] = (unsigned char)MND_MERGE_KEY_NO_MSGCTXT;
        pos++;
    }

    dst[pos] = (unsigned char)MND_MERGE_KEY_SEPARATOR;
    pos++;
    if (entry->msgid.len > 0)
    {
        memcpy(dst + pos, entry->msgid.data, entry->msgid.len);
    }
}

/* entry のキーの連結を key（空）に作る。false はメモリ不足 */
static bool key_build(mnd_buf_t *key, const mnd_po_entry_t *entry)
{
    size_t len = 0;

    if (!key_length(entry, &len))
    {
        return false;
    }

    if (!mnd_buf_reserve(key, len))
    {
        return false;
    }

    key_write((unsigned char *)key->data, entry);
    key->len = len;
    key->data[len] = '\0';

    return true;
}

bool mnd_merge_key_index_init(mnd_merge_key_index_t *index, const mnd_po_catalog_t *catalog)
{
    size_t i;
    size_t len = 0;

    index->catalog = catalog;
    mnd_merge_hash_init(&index->hash);
    index->keys = NULL;
    index->key_count = 0;
    index->max_key_len = 0;
    index->work = NULL;

    if (catalog->count == 0)
    {
        return true;
    }

    /* 検索のキーの作業領域の大きさを決めるため、すべてのエントリのキーの長さの最大を求める */
    for (i = 0; i < catalog->count; i++)
    {
        if (!key_length(&catalog->entries[i], &len))
        {
            return false;
        }

        if (len > index->max_key_len)
        {
            index->max_key_len = len;
        }
    }

    if (catalog->count > (SIZE_MAX / sizeof(mnd_buf_t)))
    {
        return false;
    }

    index->keys = malloc(catalog->count * sizeof(mnd_buf_t));
    if (index->keys == NULL)
    {
        return false;
    }

    for (i = 0; i < catalog->count; i++)
    {
        mnd_buf_init(&index->keys[i]);
    }

    index->key_count = catalog->count;

    /* キーの長さは常に 2 以上なので max_key_len は 1 以上 */
    index->work = malloc(index->max_key_len);
    if (index->work == NULL)
    {
        mnd_merge_key_index_free(index);

        return false;
    }

    return true;
}

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
    free(index->work);
    index->work = NULL;
    index->max_key_len = 0;
    index->catalog = NULL;
}

bool mnd_merge_key_index_add(mnd_merge_key_index_t *index, size_t entry_index,
                             bool *added, size_t *existing)
{
    mnd_buf_t *key;
    bool inserted = false;
    size_t earlier = 0;

    if (entry_index >= index->key_count)
    {
        return false;
    }

    key = &index->keys[entry_index];
    /* キーの長さは 2 以上なので、len == 0 はまだ作っていないことを表す */
    if (key->len == 0)
    {
        if (!key_build(key, &index->catalog->entries[entry_index]))
        {
            return false;
        }
    }

    if (!mnd_merge_hash_insert(&index->hash, key->data, key->len, entry_index, &inserted,
                               &earlier))
    {
        return false;
    }

    if (!inserted)
    {
        /* 表に入らなかったキーは表から指されないので解放する（同じ添字の 2 回目の登録を除く） */
        if (earlier != entry_index)
        {
            mnd_buf_free(key);
        }

        *existing = earlier;
    }

    *added = inserted;

    return true;
}

bool mnd_merge_key_index_find(const mnd_merge_key_index_t *index, const mnd_po_entry_t *key,
                              size_t *entry_index)
{
    size_t len = 0;
    size_t value = 0;

    /*
     * カタログのどのキーより長いキーは、登録されたどのキーとも一致しない。
     * そうでなければ作業領域に収まるので、領域を確保せずに検索できる
     */
    if (!key_length(key, &len) || (len > index->max_key_len))
    {
        return false;
    }

    key_write(index->work, key);
    if (!mnd_merge_hash_find(&index->hash, index->work, len, &value))
    {
        return false;
    }

    *entry_index = value;

    return true;
}
