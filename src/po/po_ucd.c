/* Ucd: 表の検索（行分割クラス、文字幅、Mn・Mc の判定） */

#include "po/po_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ranges[0..count)（first の昇順で重ならない）から cp を含む範囲を二分探索する。
 * 見つかれば *value に値を入れて true を返す
 */
static bool mnd_po_ucd_find(const mnd_po_ucd_range_t *ranges, size_t count, uint32_t cp,
                            unsigned char *value)
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
            *value = ranges[mid].value;

            return true;
        }
    }

    return false;
}

mnd_po_lb_t mnd_po_ucd_lb(uint32_t cp)
{
    unsigned char value = 0;

    if (mnd_po_ucd_find(mnd_po_ucd_lb_ranges, mnd_po_ucd_lb_count, cp, &value))
    {
        return (mnd_po_lb_t)value;
    }

    return MND_PO_LB_XX;
}

int mnd_po_ucd_width(uint32_t cp)
{
    unsigned char value = 0;

    if (mnd_po_ucd_find(mnd_po_ucd_width_ranges, mnd_po_ucd_width_count, cp, &value))
    {
        return (int)value;
    }

    return 1;
}

bool mnd_po_ucd_is_mark(uint32_t cp)
{
    unsigned char value = 0;

    return mnd_po_ucd_find(mnd_po_ucd_mark_ranges, mnd_po_ucd_mark_count, cp, &value);
}
