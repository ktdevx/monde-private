/* Entity: 実体名の検索（二分探索） */

#include "markdown/md.h"
#include "markdown/md_entity.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * name[0..len) と表の項目の名前を、バイト列として比べる（strcmp と同じ順）。
 * name が項目より前なら負、後ろなら正、等しければ 0 を返す
 */
static int mnd_md_entity_compare(const char *name, size_t len, const mnd_md_entity_t *entry)
{
    size_t n = (len < entry->name_len) ? len : entry->name_len;
    int c = memcmp(name, entry->name, n);

    if (c != 0)
    {
        return c;
    }

    if (len < entry->name_len)
    {
        return -1;
    }

    if (len > entry->name_len)
    {
        return 1;
    }

    return 0;
}

bool mnd_md_entity_lookup(const char *name, size_t len, const unsigned char **utf8, size_t *utf8_len)
{
    size_t lo = 0;
    size_t hi = mnd_md_entity_count;

    if ((len == 0) || (len > MND_MD_ENTITY_NAME_MAX))
    {
        return false;
    }

    while (lo < hi)
    {
        size_t mid = lo + ((hi - lo) / 2);
        int c = mnd_md_entity_compare(name, len, &mnd_md_entity_entries[mid]);

        if (c < 0)
        {
            hi = mid;
        }
        else if (c > 0)
        {
            lo = mid + 1;
        }
        else
        {
            *utf8 = mnd_md_entity_entries[mid].utf8;
            *utf8_len = mnd_md_entity_entries[mid].len;
            return true;
        }
    }

    return false;
}
