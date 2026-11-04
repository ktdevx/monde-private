/* Fuzzy: 候補の登録、4-gram の索引、バイト長ごとの列、候補を調べる順序と最良の候補の選択 */

#include "po_merge/merge_internal.h"

#include <stdint.h>
#include <stdlib.h>

/* 列と 4-gram の列の配列を初めて確保するときの容量 */
#define MND_MERGE_FUZZY_INITIAL_CAP 4u

static void mnd_merge_index_list_init(mnd_merge_index_list_t *list)
{
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static void mnd_merge_index_list_free(mnd_merge_index_list_t *list)
{
    free(list->items);
    mnd_merge_index_list_init(list);
}

/*
 * count 個の要素（大きさ size）を持つ容量 *cap の領域に、もう 1 個入るように必要なら容量を 2 倍にする。
 * *items と *cap を更新する。失敗時は領域を変えずに false
 */
static bool grow_capacity(void **items, size_t count, size_t *cap, size_t size)
{
    size_t new_cap;
    void *grown;

    if (count < *cap)
    {
        return true;
    }

    if (*cap == 0)
    {
        new_cap = MND_MERGE_FUZZY_INITIAL_CAP;
    }
    else
    {
        if (*cap > (SIZE_MAX / 2))
        {
            return false;
        }
        new_cap = *cap * 2;
    }

    if (new_cap > (SIZE_MAX / size))
    {
        return false;
    }

    grown = realloc(*items, new_cap * size);
    if (grown == NULL)
    {
        return false;
    }

    *items = grown;
    *cap = new_cap;

    return true;
}

/* list の末尾に value を加える。失敗時は list を変えずに false */
static bool index_list_push(mnd_merge_index_list_t *list, size_t value)
{
    void *items = list->items;

    if (!grow_capacity(&items, list->count, &list->cap, sizeof(size_t)))
    {
        return false;
    }

    list->items = items;
    list->items[list->count] = value;
    list->count++;

    return true;
}

/* 訳がある（msgstr が 1 個以上で、最初の msgstr が空でない）。Builder に依存しないため Fuzzy で判定する */
static bool fuzzy_has_translation(const mnd_po_entry_t *entry)
{
    return (entry->msgstr.count > 0) && (entry->msgstr.items[0].len > 0);
}

/* pos（len 未満）から始まる UTF-8 の文字の次の文字の先頭（または len）を返す */
static size_t next_char(const unsigned char *s, size_t len, size_t pos)
{
    pos++;
    while ((pos < len) && ((s[pos] & 0xC0u) == 0x80u))
    {
        pos++;
    }

    return pos;
}

/*
 * 4-gram（s の start から end の前まで。s は候補の msgid の内部）の列に候補の番号 candidate を加える。
 * 候補は番号の昇順に登録するので、列の末尾が同じ番号なら加えない（列は昇順、重複なし）
 */
static bool add_gram(mnd_merge_fuzzy_t *fuzzy, const unsigned char *gram, size_t gram_len,
                     size_t candidate)
{
    mnd_merge_index_list_t *list;
    void *lists = fuzzy->gram_lists;
    bool inserted = false;
    size_t existing = 0;
    size_t id;

    /* 新しい 4-gram の列を作れるように、ハッシュ表に入れる前に配列の容量を確保する */
    if (!grow_capacity(&lists, fuzzy->gram_list_count, &fuzzy->gram_list_cap,
                       sizeof(mnd_merge_index_list_t)))
    {
        return false;
    }
    fuzzy->gram_lists = lists;

    if (!mnd_merge_hash_insert(&fuzzy->grams, gram, gram_len, fuzzy->gram_list_count,
                               &inserted, &existing))
    {
        return false;
    }

    if (inserted)
    {
        id = fuzzy->gram_list_count;
        mnd_merge_index_list_init(&fuzzy->gram_lists[id]);
        fuzzy->gram_list_count++;
    }
    else
    {
        id = existing;
    }

    list = &fuzzy->gram_lists[id];
    if ((list->count > 0) && (list->items[list->count - 1] == candidate))
    {
        return true;
    }

    return index_list_push(list, candidate);
}

/* 候補の番号 candidate の msgid のすべての 4-gram を索引に加える。4 文字未満なら何もしない */
static bool add_grams(mnd_merge_fuzzy_t *fuzzy, const mnd_buf_t *msgid, size_t candidate)
{
    const unsigned char *s = (const unsigned char *)msgid->data;
    size_t len = msgid->len;
    size_t start = 0;
    size_t end = 0;
    size_t chars = 0;

    /* end を start から 4 文字先に進める */
    while ((chars < MND_MERGE_FUZZY_GRAM_CHARS) && (end < len))
    {
        end = next_char(s, len, end);
        chars++;
    }

    if (chars < MND_MERGE_FUZZY_GRAM_CHARS)
    {
        return true;
    }

    for (;;)
    {
        if (!add_gram(fuzzy, s + start, end - start, candidate))
        {
            return false;
        }

        if (end >= len)
        {
            break;
        }

        start = next_char(s, len, start);
        end = next_char(s, len, end);
    }

    return true;
}

/* 空の状態にする（解放はしない） */
static void fuzzy_set_empty(mnd_merge_fuzzy_t *fuzzy, const mnd_po_catalog_t *catalog)
{
    size_t i;

    fuzzy->catalog = catalog;
    fuzzy->candidates = NULL;
    fuzzy->candidate_count = 0;
    mnd_merge_hash_init(&fuzzy->grams);
    fuzzy->gram_lists = NULL;
    fuzzy->gram_list_count = 0;
    fuzzy->gram_list_cap = 0;
    for (i = 0; i <= MND_MERGE_FUZZY_SHORT_MAX_LEN; i++)
    {
        mnd_merge_index_list_init(&fuzzy->by_length[i]);
    }
    fuzzy->counts = NULL;
    fuzzy->touched = NULL;
    fuzzy->touched_count = 0;
    fuzzy->pattern.len = 0;
    fuzzy->pattern.words = 0;
    fuzzy->pattern.word_cap = 0;
    fuzzy->pattern.masks = NULL;
    fuzzy->pattern.vector = NULL;
}

/* 候補（訳のあるエントリ、ヘッダを除く）の添字を昇順に candidates に入れ、作業領域を確保する */
static bool register_candidates(mnd_merge_fuzzy_t *fuzzy)
{
    const mnd_po_catalog_t *catalog = fuzzy->catalog;
    size_t header = 0;
    bool has_header = mnd_po_catalog_header(catalog, &header);
    size_t count = 0;
    size_t i;

    for (i = 0; i < catalog->count; i++)
    {
        if ((!has_header || (i != header)) && fuzzy_has_translation(&catalog->entries[i]))
        {
            count++;
        }
    }

    if (count == 0)
    {
        return true;
    }

    if (count > (SIZE_MAX / sizeof(size_t)))
    {
        return false;
    }

    fuzzy->candidates = malloc(count * sizeof(size_t));
    fuzzy->counts = calloc(count, sizeof(size_t));
    fuzzy->touched = malloc(count * sizeof(size_t));
    if ((fuzzy->candidates == NULL) || (fuzzy->counts == NULL) || (fuzzy->touched == NULL))
    {
        return false;
    }

    for (i = 0; i < catalog->count; i++)
    {
        if ((!has_header || (i != header)) && fuzzy_has_translation(&catalog->entries[i]))
        {
            fuzzy->candidates[fuzzy->candidate_count] = i;
            fuzzy->candidate_count++;
        }
    }

    return true;
}

bool mnd_merge_fuzzy_init(mnd_merge_fuzzy_t *fuzzy, const mnd_po_catalog_t *catalog)
{
    size_t n;

    fuzzy_set_empty(fuzzy, catalog);

    if (!register_candidates(fuzzy))
    {
        mnd_merge_fuzzy_free(fuzzy);

        return false;
    }

    /* 候補の番号の昇順に加えるので、どの列も昇順になる */
    for (n = 0; n < fuzzy->candidate_count; n++)
    {
        const mnd_buf_t *msgid = &catalog->entries[fuzzy->candidates[n]].msgid;

        if (msgid->len <= MND_MERGE_FUZZY_SHORT_MAX_LEN)
        {
            if (!index_list_push(&fuzzy->by_length[msgid->len], n))
            {
                mnd_merge_fuzzy_free(fuzzy);

                return false;
            }
        }

        if (!add_grams(fuzzy, msgid, n))
        {
            mnd_merge_fuzzy_free(fuzzy);

            return false;
        }
    }

    return true;
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
