/* Unicode: 一般カテゴリの判定、大文字小文字の畳み込み */

#include "markdown/md.h"
#include "markdown/md_unicode.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ranges[0..count)（first の昇順で重ならない）に cp を含む範囲があれば true を返す */
static bool mnd_md_unicode_in_ranges(const mnd_md_unicode_range_t *ranges, size_t count, uint32_t cp)
{
    size_t lo = 0;
    size_t hi = count;

    while (lo < hi)
    {
        size_t mid = lo + ((hi - lo) / 2);

        if (cp < ranges[mid].first)
        {
            hi = mid;
        }
        else if (cp > ranges[mid].last)
        {
            lo = mid + 1;
        }
        else
        {
            return true;
        }
    }

    return false;
}

/* 畳み込みの表から cp の項目を二分探索する。なければ NULL を返す */
static const mnd_md_unicode_fold_entry_t *mnd_md_unicode_find_fold(uint32_t cp)
{
    size_t lo = 0;
    size_t hi = mnd_md_unicode_fold_count;

    while (lo < hi)
    {
        size_t mid = lo + ((hi - lo) / 2);

        if (cp < mnd_md_unicode_fold_entries[mid].cp)
        {
            hi = mid;
        }
        else if (cp > mnd_md_unicode_fold_entries[mid].cp)
        {
            lo = mid + 1;
        }
        else
        {
            return &mnd_md_unicode_fold_entries[mid];
        }
    }

    return NULL;
}

bool mnd_md_unicode_is_punct(uint32_t cp)
{
    return mnd_md_unicode_in_ranges(mnd_md_unicode_punct_ranges, mnd_md_unicode_punct_count, cp);
}

bool mnd_md_unicode_is_space(uint32_t cp)
{
    return mnd_md_unicode_in_ranges(mnd_md_unicode_space_ranges, mnd_md_unicode_space_count, cp);
}

size_t mnd_md_unicode_fold(uint32_t cp, char out[MND_MD_UNICODE_FOLD_MAX])
{
    const mnd_md_unicode_fold_entry_t *entry = NULL;
    size_t i = 0;

    if ((cp > 0x10FFFFu) || ((cp >= 0xD800u) && (cp <= 0xDFFFu)))
    {
        return 0;
    }

    entry = mnd_md_unicode_find_fold(cp);
    if (entry != NULL)
    {
        for (i = 0; i < entry->len; i++)
        {
            out[i] = (char)entry->utf8[i];
        }

        return entry->len;
    }

    if (cp < 0x80u)
    {
        out[0] = (char)cp;

        return 1;
    }
    if (cp < 0x800u)
    {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));

        return 2;
    }
    if (cp < 0x10000u)
    {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));

        return 3;
    }
    out[0] = (char)(0xF0u | (cp >> 18));
    out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (char)(0x80u | (cp & 0x3Fu));

    return 4;
}
