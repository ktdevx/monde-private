/* Hash: バイト列をキーとする開番地法のハッシュ表（FNV-1a、容量は 2 の累乗、使用率 1/2 以下） */

#include "po_merge/merge_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 初めて確保するときの容量（2 の累乗） */
#define MND_MERGE_HASH_INITIAL_CAP 16u

/* FNV-1a（64 ビット）の初期値と乗数 */
#define MND_MERGE_HASH_FNV_OFFSET UINT64_C(14695981039346656037)
#define MND_MERGE_HASH_FNV_PRIME UINT64_C(1099511628211)

static uint64_t hash_bytes(const unsigned char *key, size_t len)
{
    uint64_t h = MND_MERGE_HASH_FNV_OFFSET;
    size_t i;

    for (i = 0; i < len; i++)
    {
        h ^= (uint64_t)key[i];
        h *= MND_MERGE_HASH_FNV_PRIME;
    }

    return h;
}

static bool slot_matches(const mnd_merge_hash_slot_t *slot, const unsigned char *key,
                         size_t len, uint64_t h)
{
    if ((slot->hash != h) || (slot->len != len))
    {
        return false;
    }

    /* 長さ 0 のキーはポインタ（NULL を含む）によらず等しい */
    if (len == 0)
    {
        return true;
    }

    return memcmp(slot->key, key, len) == 0;
}

/*
 * h のキーの枠を探す。同じキーの枠があればその位置、なければ最初の空の枠の位置を返す。
 * cap は 2 の累乗で、空の枠が 1 つ以上あること
 */
static size_t probe(const mnd_merge_hash_slot_t *slots, size_t cap, const unsigned char *key,
                    size_t len, uint64_t h)
{
    size_t mask = cap - 1;
    size_t i = (size_t)h & mask;

    while (slots[i].used && !slot_matches(&slots[i], key, len, h))
    {
        i = (i + 1) & mask;
    }

    return i;
}

/* 容量を new_cap（2 の累乗）にして全キーを入れ直す。失敗時は表を変えずに false */
static bool resize(mnd_merge_hash_t *hash, size_t new_cap)
{
    mnd_merge_hash_slot_t *slots;
    size_t i;

    if (new_cap > (SIZE_MAX / sizeof(*slots)))
    {
        return false;
    }

    slots = calloc(new_cap, sizeof(*slots));
    if (slots == NULL)
    {
        return false;
    }

    for (i = 0; i < hash->cap; i++)
    {
        if (hash->slots[i].used)
        {
            const mnd_merge_hash_slot_t *old = &hash->slots[i];
            size_t mask = new_cap - 1;
            size_t j = (size_t)old->hash & mask;

            /* 新しい表のキーはすべて異なるので、空の枠まで進めばよい */
            while (slots[j].used)
            {
                j = (j + 1) & mask;
            }

            slots[j] = *old;
        }
    }

    free(hash->slots);
    hash->slots = slots;
    hash->cap = new_cap;

    return true;
}

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

bool mnd_merge_hash_insert(mnd_merge_hash_t *hash, const void *key, size_t len,
                           size_t value, bool *inserted, size_t *existing)
{
    const unsigned char *bytes = key;
    uint64_t h = hash_bytes(bytes, len);
    size_t i;

    if (hash->cap > 0)
    {
        i = probe(hash->slots, hash->cap, bytes, len, h);
        if (hash->slots[i].used)
        {
            *inserted = false;
            *existing = hash->slots[i].value;

            return true;
        }
    }

    /* 挿入の後も使用率を 1/2 以下に保つ: (count + 1) * 2 <= cap */
    if (hash->count >= (hash->cap / 2))
    {
        size_t new_cap;

        if (hash->cap == 0)
        {
            new_cap = MND_MERGE_HASH_INITIAL_CAP;
        }
        else
        {
            if (hash->cap > (SIZE_MAX / 2))
            {
                return false;
            }

            new_cap = hash->cap * 2;
        }

        if (!resize(hash, new_cap))
        {
            return false;
        }
    }

    i = probe(hash->slots, hash->cap, bytes, len, h);
    hash->slots[i].key = bytes;
    hash->slots[i].len = len;
    hash->slots[i].hash = h;
    hash->slots[i].value = value;
    hash->slots[i].used = true;
    hash->count++;
    *inserted = true;

    return true;
}

bool mnd_merge_hash_find(const mnd_merge_hash_t *hash, const void *key, size_t len,
                         size_t *value)
{
    const unsigned char *bytes = key;
    uint64_t h;
    size_t i;

    if (hash->cap == 0)
    {
        return false;
    }

    h = hash_bytes(bytes, len);
    i = probe(hash->slots, hash->cap, bytes, len, h);
    if (!hash->slots[i].used)
    {
        return false;
    }

    *value = hash->slots[i].value;

    return true;
}
