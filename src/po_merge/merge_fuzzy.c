/* Fuzzy: 候補の登録、4-gram の索引、バイト長ごとの列、候補を調べる順序と最良の候補の選択 */

#include "po_merge/merge_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 列と 4-gram の列の配列を初めて確保するときの容量 */
#define MND_MERGE_FUZZY_INITIAL_CAP 4u

/* 比較値のしきい値（最良値の初期値）。この値を超えた候補だけを選ぶ */
#define MND_MERGE_FUZZY_THRESHOLD 0.6

/* msgctxt を持たない候補と、問い合わせと同じ msgctxt を持つ候補の比較値に加える値 */
#define MND_MERGE_FUZZY_BONUS 0.00001

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

/* s の先頭から 4 文字先の位置を *end に入れる。4 文字に満たなければ false */
static bool first_gram_end(const unsigned char *s, size_t len, size_t *end)
{
    size_t pos = 0;
    size_t chars = 0;

    while ((chars < MND_MERGE_FUZZY_GRAM_CHARS) && (pos < len))
    {
        pos = next_char(s, len, pos);
        chars++;
    }

    *end = pos;

    return chars >= MND_MERGE_FUZZY_GRAM_CHARS;
}

/* 候補の番号 candidate の msgid のすべての 4-gram を索引に加える。4 文字未満なら何もしない */
static bool add_grams(mnd_merge_fuzzy_t *fuzzy, const mnd_buf_t *msgid, size_t candidate)
{
    const unsigned char *s = (const unsigned char *)msgid->data;
    size_t len = msgid->len;
    size_t start = 0;
    size_t end = 0;

    if (!first_gram_end(s, len, &end))
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

/* 候補の番号 a を b より先に調べる（数え上げの降順、同数なら番号の昇順。全順序） */
static bool order_before(const size_t *counts, size_t a, size_t b)
{
    if (counts[a] != counts[b])
    {
        return counts[a] > counts[b];
    }

    return a < b;
}

/* items[start..end) のヒープで、start の要素を子の方へ沈める（後に調べる候補ほど根に近い） */
static void sift_down(size_t *items, size_t start, size_t end, const size_t *counts)
{
    size_t root = start;

    for (;;)
    {
        size_t child;
        size_t tmp;

        /* end <= candidate_count <= SIZE_MAX / sizeof(size_t) なので 2 × root + 2 はオーバーフローしない */
        child = (2 * root) + 1;
        if (child >= end)
        {
            break;
        }

        if (((child + 1) < end) && order_before(counts, items[child], items[child + 1]))
        {
            child++;
        }

        if (!order_before(counts, items[root], items[child]))
        {
            break;
        }

        tmp = items[root];
        items[root] = items[child];
        items[child] = tmp;
        root = child;
    }
}

/* touched を調べる順に並べる（ヒープソート。全順序なので結果は一意） */
static void sort_touched(mnd_merge_fuzzy_t *fuzzy)
{
    size_t *items = fuzzy->touched;
    size_t n = fuzzy->touched_count;
    size_t i;
    size_t end;

    if (n < 2)
    {
        return;
    }

    for (i = n / 2; i > 0; i--)
    {
        sift_down(items, i - 1, n, fuzzy->counts);
    }

    for (end = n - 1; end > 0; end--)
    {
        size_t tmp = items[0];

        items[0] = items[end];
        items[end] = tmp;
        sift_down(items, 0, end, fuzzy->counts);
    }
}

/* 4 文字以上の msgid の 4-gram の各出現で候補を数え上げ、数え上げが 1 以上の候補を touched に入れる */
static void count_grams(mnd_merge_fuzzy_t *fuzzy, const unsigned char *s, size_t len, size_t end)
{
    size_t start = 0;

    for (;;)
    {
        size_t id = 0;

        if (mnd_merge_hash_find(&fuzzy->grams, s + start, end - start, &id))
        {
            const mnd_merge_index_list_t *list = &fuzzy->gram_lists[id];
            size_t i;

            for (i = 0; i < list->count; i++)
            {
                size_t n = list->items[i];

                if (fuzzy->counts[n] == 0)
                {
                    fuzzy->touched[fuzzy->touched_count] = n;
                    fuzzy->touched_count++;
                }

                fuzzy->counts[n]++;
            }
        }

        if (end >= len)
        {
            break;
        }

        start = next_char(s, len, start);
        end = next_char(s, len, end);
    }
}

/*
 * 4 文字未満の msgid（バイト長 len）について、k = 2.0 / 0.6 − 1.0 として、バイト長 ceil(len / k)
 * から (size_t)(len × k) まで（28 以下）の列を、バイト長の昇順に touched に入れる。
 * ceil は double の商の整数部分と比べて切り上げる（浮動小数点の数学関数を使わない）
 */
static void collect_short(mnd_merge_fuzzy_t *fuzzy, size_t len)
{
    const double k = (2.0 / MND_MERGE_FUZZY_THRESHOLD) - 1.0;
    const double max_len = (double)MND_MERGE_FUZZY_SHORT_MAX_LEN;
    double lower_value = (double)len / k;
    double upper_value = (double)len * k;
    size_t lower;
    size_t upper;
    size_t l;

    /* 28 を超える長さの列はないので、変換のオーバーフローを避けて先に除く */
    if (lower_value > max_len)
    {
        return;
    }

    lower = (size_t)lower_value;
    if ((double)lower < lower_value)
    {
        lower++;
    }

    if (upper_value >= max_len)
    {
        upper = MND_MERGE_FUZZY_SHORT_MAX_LEN;
    }
    else
    {
        upper = (size_t)upper_value;
    }

    for (l = lower; l <= upper; l++)
    {
        const mnd_merge_index_list_t *list = &fuzzy->by_length[l];
        size_t i;

        /* 各候補は 1 つの列にだけあるので、合計は candidate_count を超えない */
        for (i = 0; i < list->count; i++)
        {
            fuzzy->touched[fuzzy->touched_count] = list->items[i];
            fuzzy->touched_count++;
        }
    }
}

void mnd_merge_fuzzy_order(mnd_merge_fuzzy_t *fuzzy, const mnd_po_entry_t *query)
{
    const unsigned char *s = (const unsigned char *)query->msgid.data;
    size_t len = query->msgid.len;
    size_t end = 0;
    size_t i;

    fuzzy->touched_count = 0;
    if (fuzzy->candidate_count == 0)
    {
        return;
    }

    if (first_gram_end(s, len, &end))
    {
        count_grams(fuzzy, s, len, end);
        sort_touched(fuzzy);
        for (i = 0; i < fuzzy->touched_count; i++)
        {
            fuzzy->counts[fuzzy->touched[i]] = 0;
        }
    }
    else
    {
        collect_short(fuzzy, len);
    }
}

/* 候補 candidate が msgctxt を持たない、または query が candidate と同じ msgctxt を持つ */
static bool has_bonus(const mnd_po_entry_t *candidate, const mnd_po_entry_t *query)
{
    if (!candidate->has_msgctxt)
    {
        return true;
    }

    if (!query->has_msgctxt || (query->msgctxt.len != candidate->msgctxt.len))
    {
        return false;
    }

    return (query->msgctxt.len == 0)
        || (memcmp(query->msgctxt.data, candidate->msgctxt.data, query->msgctxt.len) == 0);
}

bool mnd_merge_fuzzy_search(mnd_merge_fuzzy_t *fuzzy, const mnd_po_entry_t *query,
                            bool *has_found, size_t *found)
{
    const char *q = query->msgid.data;
    size_t qlen = query->msgid.len;
    double best = MND_MERGE_FUZZY_THRESHOLD;
    bool has_best = false;
    size_t best_index = 0;
    size_t i;

    *has_found = false;
    *found = 0;

    if (fuzzy->candidate_count == 0)
    {
        return true;
    }

    /* 問い合わせのマスクを 1 回作る（数え上げより前に作り、失敗しても作業領域を汚さない） */
    if (!mnd_merge_pattern_set(&fuzzy->pattern, q, qlen))
    {
        return false;
    }

    mnd_merge_fuzzy_order(fuzzy, query);

    for (i = 0; i < fuzzy->touched_count; i++)
    {
        size_t entry_index = fuzzy->candidates[fuzzy->touched[i]];
        const mnd_po_entry_t *candidate = &fuzzy->catalog->entries[entry_index];
        const char *c = candidate->msgid.data;
        size_t clen = candidate->msgid.len;
        size_t sum = qlen + clen;
        double bonus = has_bonus(candidate, query) ? MND_MERGE_FUZZY_BONUS : 0.0;
        volatile double upper;
        volatile double weight;
        volatile double value;

        /* 上限に bonus を加えても最良値を超えない候補は、LCS を計算せずに飛ばす */
        upper = mnd_merge_ratio(mnd_merge_upper_numerator(q, qlen, c, clen) / 2, sum);
        if ((upper + bonus) <= best)
        {
            continue;
        }

        /* 片方だけが空なら LCS は 0 で 0.0、両方空なら 1.0 */
        weight = mnd_merge_ratio(mnd_merge_lcs(&fuzzy->pattern, c, clen), sum);
        value = weight + bonus;
        if (value > best)
        {
            best = value;
            has_best = true;
            best_index = entry_index;
        }
    }

    fuzzy->touched_count = 0;
    *has_found = has_best;
    *found = best_index;

    return true;
}
