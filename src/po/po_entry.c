/* Model: 型の初期化・解放、配列の追加、状態、フラグ、検索、重複の検出 */

#include "po/po.h"
#include "po/po_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MND_PO_MIN_CAP 4

/*
 * 要素の大きさが elem_size の配列に 1 個加えるための容量を求める。
 * 容量に余裕があれば cap をそのまま返す。乗算のオーバーフローは確保の前に検査する
 */
static bool mnd_po_grow_cap(size_t count, size_t cap, size_t elem_size, size_t *out_cap)
{
    size_t new_cap;

    if (count < cap)
    {
        *out_cap = cap;

        return true;
    }

    if (cap == 0)
    {
        new_cap = MND_PO_MIN_CAP;
    }
    else
    {
        if (cap > (SIZE_MAX / 2))
        {
            return false;
        }

        new_cap = cap * 2;
    }

    if (new_cap > (SIZE_MAX / elem_size))
    {
        return false;
    }

    *out_cap = new_cap;

    return true;
}

static void mnd_po_strings_free(mnd_po_strings_t *list)
{
    size_t i;

    for (i = 0; i < list->count; i++)
    {
        mnd_buf_free(&list->items[i]);
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static void mnd_po_refs_free(mnd_po_refs_t *refs)
{
    size_t i;

    for (i = 0; i < refs->count; i++)
    {
        mnd_buf_free(&refs->items[i].file);
    }

    free(refs->items);
    refs->items = NULL;
    refs->count = 0;
    refs->cap = 0;
}

void mnd_po_entry_init(mnd_po_entry_t *entry)
{
    entry->translator_comments.items = NULL;
    entry->translator_comments.count = 0;
    entry->translator_comments.cap = 0;
    entry->extracted_comments = entry->translator_comments;
    entry->flags = entry->translator_comments;
    entry->msgstr = entry->translator_comments;
    entry->refs.items = NULL;
    entry->refs.count = 0;
    entry->refs.cap = 0;
    entry->has_prev_msgctxt = false;
    mnd_buf_init(&entry->prev_msgctxt);
    entry->has_prev_msgid = false;
    mnd_buf_init(&entry->prev_msgid);
    entry->has_prev_msgid_plural = false;
    mnd_buf_init(&entry->prev_msgid_plural);
    entry->has_msgctxt = false;
    mnd_buf_init(&entry->msgctxt);
    mnd_buf_init(&entry->msgid);
    entry->has_msgid_plural = false;
    mnd_buf_init(&entry->msgid_plural);
    entry->obsolete = false;
    entry->line = 0;
}

void mnd_po_entry_free(mnd_po_entry_t *entry)
{
    mnd_po_strings_free(&entry->translator_comments);
    mnd_po_strings_free(&entry->extracted_comments);
    mnd_po_refs_free(&entry->refs);
    mnd_po_strings_free(&entry->flags);
    mnd_buf_free(&entry->prev_msgctxt);
    mnd_buf_free(&entry->prev_msgid);
    mnd_buf_free(&entry->prev_msgid_plural);
    mnd_buf_free(&entry->msgctxt);
    mnd_buf_free(&entry->msgid);
    mnd_buf_free(&entry->msgid_plural);
    mnd_po_strings_free(&entry->msgstr);
    mnd_po_entry_init(entry);
}

bool mnd_po_strings_push(mnd_po_strings_t *list, const char *data, size_t len)
{
    size_t new_cap;
    mnd_buf_t *items;
    mnd_buf_t item;

    if (!mnd_po_grow_cap(list->count, list->cap, sizeof(mnd_buf_t), &new_cap))
    {
        return false;
    }

    mnd_buf_init(&item);
    if (!mnd_buf_append(&item, data, len))
    {
        mnd_buf_free(&item);

        return false;
    }

    if (new_cap != list->cap)
    {
        items = (mnd_buf_t *)realloc(list->items, new_cap * sizeof(mnd_buf_t));
        if (items == NULL)
        {
            mnd_buf_free(&item);

            return false;
        }

        list->items = items;
        list->cap = new_cap;
    }

    list->items[list->count] = item;
    list->count++;

    return true;
}

bool mnd_po_refs_push(mnd_po_refs_t *refs, const char *file, size_t line)
{
    size_t new_cap;
    mnd_po_ref_t *items;
    mnd_po_ref_t item;

    if (!mnd_po_grow_cap(refs->count, refs->cap, sizeof(mnd_po_ref_t), &new_cap))
    {
        return false;
    }

    mnd_buf_init(&item.file);
    item.line = line;
    if (!mnd_buf_append_str(&item.file, file))
    {
        mnd_buf_free(&item.file);

        return false;
    }

    if (new_cap != refs->cap)
    {
        items = (mnd_po_ref_t *)realloc(refs->items, new_cap * sizeof(mnd_po_ref_t));
        if (items == NULL)
        {
            mnd_buf_free(&item.file);

            return false;
        }

        refs->items = items;
        refs->cap = new_cap;
    }

    refs->items[refs->count] = item;
    refs->count++;

    return true;
}

void mnd_po_catalog_init(mnd_po_catalog_t *catalog)
{
    catalog->entries = NULL;
    catalog->count = 0;
    catalog->cap = 0;
}

void mnd_po_catalog_free(mnd_po_catalog_t *catalog)
{
    size_t i;

    for (i = 0; i < catalog->count; i++)
    {
        mnd_po_entry_free(&catalog->entries[i]);
    }

    free(catalog->entries);
    mnd_po_catalog_init(catalog);
}

mnd_po_entry_t *mnd_po_catalog_append(mnd_po_catalog_t *catalog)
{
    return mnd_po_catalog_insert(catalog, catalog->count);
}

mnd_po_entry_t *mnd_po_catalog_insert(mnd_po_catalog_t *catalog, size_t index)
{
    size_t new_cap;
    mnd_po_entry_t *entries;

    if (index > catalog->count)
    {
        return NULL;
    }

    if (!mnd_po_grow_cap(catalog->count, catalog->cap, sizeof(mnd_po_entry_t), &new_cap))
    {
        return NULL;
    }

    if (new_cap != catalog->cap)
    {
        entries = (mnd_po_entry_t *)realloc(catalog->entries,
                                            new_cap * sizeof(mnd_po_entry_t));
        if (entries == NULL)
        {
            return NULL;
        }

        catalog->entries = entries;
        catalog->cap = new_cap;
    }

    if (index < catalog->count)
    {
        memmove(&catalog->entries[index + 1], &catalog->entries[index],
                (catalog->count - index) * sizeof(mnd_po_entry_t));
    }

    mnd_po_entry_init(&catalog->entries[index]);
    catalog->count++;

    return &catalog->entries[index];
}

/* 1.5・5.7・5.8 で共通の「最初の msgstr が空でない」の判定（po_internal.h） */
bool mnd_po_entry_first_msgstr_is_nonempty(const mnd_po_entry_t *entry)
{
    return entry->msgstr.count > 0 && entry->msgstr.items[0].len > 0;
}

mnd_po_state_t mnd_po_entry_state(const mnd_po_entry_t *entry)
{
    size_t i;

    if (mnd_po_flag_has(entry, "fuzzy"))
    {
        return mnd_po_entry_first_msgstr_is_nonempty(entry) ? MND_PO_STATE_FUZZY
                                                            : MND_PO_STATE_UNTRANSLATED;
    }

    if (entry->msgstr.count == 0)
    {
        return MND_PO_STATE_UNTRANSLATED;
    }

    for (i = 0; i < entry->msgstr.count; i++)
    {
        if (entry->msgstr.items[i].len == 0)
        {
            return MND_PO_STATE_UNTRANSLATED;
        }
    }

    return MND_PO_STATE_TRANSLATED;
}

/* buf の内容が NUL 終端の str とバイト単位で完全に一致するか */
static bool mnd_po_buf_equals(const mnd_buf_t *buf, const char *str)
{
    size_t len = strlen(str);

    return (buf->len == len) && ((len == 0) || (memcmp(buf->data, str, len) == 0));
}

static bool mnd_po_buf_same(const mnd_buf_t *a, const mnd_buf_t *b)
{
    return (a->len == b->len) && ((a->len == 0) || (memcmp(a->data, b->data, a->len) == 0));
}

bool mnd_po_flag_has(const mnd_po_entry_t *entry, const char *flag)
{
    size_t i;

    for (i = 0; i < entry->flags.count; i++)
    {
        if (mnd_po_buf_equals(&entry->flags.items[i], flag))
        {
            return true;
        }
    }

    return false;
}

bool mnd_po_flag_add(mnd_po_entry_t *entry, const char *flag)
{
    if (mnd_po_flag_has(entry, flag))
    {
        return true;
    }

    return mnd_po_strings_push(&entry->flags, flag, strlen(flag));
}

void mnd_po_flag_remove(mnd_po_entry_t *entry, const char *flag)
{
    size_t i;
    size_t kept = 0;

    /* 一致するものをすべて除き、ほかの順序を保って詰める */
    for (i = 0; i < entry->flags.count; i++)
    {
        if (mnd_po_buf_equals(&entry->flags.items[i], flag))
        {
            mnd_buf_free(&entry->flags.items[i]);
        }
        else
        {
            if (kept != i)
            {
                entry->flags.items[kept] = entry->flags.items[i];
            }

            kept++;
        }
    }

    entry->flags.count = kept;
}

bool mnd_po_catalog_find(const mnd_po_catalog_t *catalog, const char *msgctxt,
                         const char *msgid, size_t *index)
{
    size_t i;
    const mnd_po_entry_t *entry;

    for (i = 0; i < catalog->count; i++)
    {
        entry = &catalog->entries[i];
        if (entry->obsolete)
        {
            continue;
        }

        if (msgctxt == NULL)
        {
            if (entry->has_msgctxt)
            {
                continue;
            }
        }
        else
        {
            if (!entry->has_msgctxt || !mnd_po_buf_equals(&entry->msgctxt, msgctxt))
            {
                continue;
            }
        }

        if (mnd_po_buf_equals(&entry->msgid, msgid))
        {
            *index = i;

            return true;
        }
    }

    return false;
}

bool mnd_po_catalog_header(const mnd_po_catalog_t *catalog, size_t *index)
{
    size_t i;
    const mnd_po_entry_t *entry;

    for (i = 0; i < catalog->count; i++)
    {
        entry = &catalog->entries[i];
        if (!entry->obsolete && !entry->has_msgctxt && (entry->msgid.len == 0))
        {
            *index = i;

            return true;
        }
    }

    return false;
}

/* msgctxt（有無を含む）と msgid が等しいか。has_msgctxt が偽なら msgctxt の内容は無視する */
static bool mnd_po_same_key(const mnd_po_entry_t *a, const mnd_po_entry_t *b)
{
    if (a->has_msgctxt != b->has_msgctxt)
    {
        return false;
    }

    if (a->has_msgctxt && !mnd_po_buf_same(&a->msgctxt, &b->msgctxt))
    {
        return false;
    }

    return mnd_po_buf_same(&a->msgid, &b->msgid);
}

/*
 * second は from 以上で、先に等しいエントリがある最小の添字。first はそのうち最小の添字（最初の定義）。
 * second + 1 を from として繰り返し呼ぶと、後に現れる重複をすべて列挙できる
 */
bool mnd_po_find_duplicate(const mnd_po_catalog_t *catalog, size_t from, size_t *first,
                           size_t *second)
{
    size_t i;
    size_t j;

    for (j = from; j < catalog->count; j++)
    {
        if (catalog->entries[j].obsolete)
        {
            continue;
        }

        for (i = 0; i < j; i++)
        {
            if (!catalog->entries[i].obsolete
                && mnd_po_same_key(&catalog->entries[i], &catalog->entries[j]))
            {
                *first = i;
                *second = j;

                return true;
            }
        }
    }

    return false;
}


size_t mnd_po_format_decimal(size_t value, size_t min_digits, char *out, size_t size)
{
    size_t digits = 1;
    size_t rest = value;
    size_t i;

    while (rest >= 10)
    {
        rest /= 10;
        digits++;
    }

    if (digits < min_digits)
    {
        digits = min_digits;
    }

    /* NUL の分を含めて収まるか（digits + 1 の桁あふれを避ける） */
    if ((size == 0) || (digits > (size - 1)))
    {
        return 0;
    }

    out[digits] = '\0';
    for (i = digits; i > 0; i--)
    {
        out[i - 1] = (char)('0' + (value % 10));
        value /= 10;
    }

    return digits;
}
