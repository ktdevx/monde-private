/* LineIndex: 行の分割（LF、CRLF、CR）と、行番号の表（foundation の数え方） */

#include "markdown/md.h"
#include "markdown/md_lines.h"

#include <string.h>

bool mnd_md_lines_next(const char *data, size_t end, size_t *pos, mnd_md_phys_line_t *out)
{
    size_t i = *pos;
    size_t eol_len = 0;

    if (i >= end)
    {
        return false;
    }

    /* NUL では止めず、end までの行末記号を探す */
    while ((i < end) && (data[i] != '\n') && (data[i] != '\r'))
    {
        i++;
    }
    if (i < end)
    {
        eol_len = 1;
        if ((data[i] == '\r') && ((i + 1) < end) && (data[i + 1] == '\n'))
        {
            eol_len = 2;
        }
    }

    out->line_start = *pos;
    out->line_end = i;
    out->eol_len = eol_len;
    *pos = i + eol_len;

    return true;
}

/* data[0..len) の LF の数 */
static size_t count_lf(const char *data, size_t len)
{
    const char *p = data;
    const char *last = data + len;
    size_t count = 0;

    /* memchr は NUL で止まらない */
    while (p < last)
    {
        p = (const char *)memchr(p, '\n', (size_t)(last - p));
        if (p == NULL)
        {
            break;
        }
        count++;
        p++;
    }

    return count;
}

bool mnd_md_line_index_build(mnd_md_line_index_t *index, const char *data, size_t len,
                             mnd_md_arena_t *arena)
{
    size_t count = 0;
    size_t *lf = NULL;
    size_t n = 0;
    size_t i = 0;

    index->lf = NULL;
    index->lf_count = 0;
    index->len = len;

    if (len == 0)
    {
        return true;
    }
    count = count_lf(data, len);
    if (count == 0)
    {
        return true;
    }
    /* 容量の計算のオーバーフローは mnd_md_arena_alloc_array が確保の前に検査する */
    lf = (size_t *)mnd_md_arena_alloc_array(arena, count, sizeof(size_t));
    if (lf == NULL)
    {
        return false;
    }
    for (i = 0; i < len; i++)
    {
        if (data[i] == '\n')
        {
            lf[n] = i;
            n++;
        }
    }
    index->lf = lf;
    index->lf_count = n;

    return true;
}

bool mnd_md_line_index_line_of(const mnd_md_line_index_t *index, size_t offset, size_t *line)
{
    size_t lo = 0;
    size_t hi = index->lf_count;
    size_t mid = 0;

    if (offset > index->len)
    {
        return false;
    }

    /*
     * offset より前（位置 < offset）にある LF の数を求める（mnd_source_line_of と同じく、
     * offset の位置にある LF はまだ数えない）。lo は「位置 >= offset の最初の LF」の添字
     */
    while (lo < hi)
    {
        mid = lo + ((hi - lo) / 2);
        if (index->lf[mid] < offset)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }

    /* lo <= lf_count。lf_count 個の size_t を確保できたので lf_count < SIZE_MAX で、lo + 1 はあふれない */
    *line = lo + 1;

    return true;
}
