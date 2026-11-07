/* Lines: 物理的な行の表（インデント、内容、行末記号、空行・コメント行の判定） */

#include "yaml/yaml_lines.h"

#include "yaml/yaml_internal.h"

#include <stddef.h>

static void lines_reset(mnd_yaml_lines_t *lines)
{
    lines->items = NULL;
    lines->count = 0;
    lines->cap = 0;
    lines->range.start = 0;
    lines->range.end = 0;
    lines->first_line = 1;
}

/*
 * data[start, end) の 1 行を調べて *line に入れる。返り値は次の行の先頭
 */
static size_t lines_scan_line(const char *data, size_t start, size_t end, size_t number, mnd_yaml_line_t *line)
{
    size_t i = start;
    size_t content_end;
    size_t next;
    size_t lead;
    unsigned char eol_len = 0;

    /* 行末記号は LF と CRLF。単独の CR は行の内容に残す */
    while ((i < end) && (data[i] != '\n'))
    {
        ++i;
    }
    if (i < end)
    {
        next = i + 1;
        if ((i > start) && (data[i - 1] == '\r'))
        {
            content_end = i - 1;
            eol_len = 2;
        }
        else
        {
            content_end = i;
            eol_len = 1;
        }
    }
    else
    {
        next = end;
        content_end = end;
    }

    line->line = number;
    line->start = start;
    line->content.start = start;
    line->content.end = content_end;
    line->eol_len = eol_len;
    line->indent = 0;
    line->tab_in_indent = false;

    /* インデントは行頭の空白の数。行頭の空白とタブの並びにタブがあれば印を付ける */
    lead = start;
    while ((lead < content_end) && (data[lead] == ' '))
    {
        ++lead;
    }
    line->indent = lead - start;
    while ((lead < content_end) && ((data[lead] == ' ') || (data[lead] == '\t')))
    {
        if (data[lead] == '\t')
        {
            line->tab_in_indent = true;
        }
        ++lead;
    }
    line->blank = (lead == content_end);
    line->comment = ((lead < content_end) && (data[lead] == '#'));

    return next;
}

bool mnd_yaml_lines_build(const char *data, size_t len, mnd_span_t content,
                          const mnd_yaml_allocator_t *allocator, mnd_yaml_lines_t *out)
{
    size_t count = 0;
    size_t first_line = 1;
    size_t bytes = 0;
    size_t pos;
    size_t i;

    lines_reset(out);
    out->range = content;
    if (!mnd_source_line_of(data, len, content.start, &first_line))
    {
        first_line = 1;
    }
    out->first_line = first_line;

    if (content.start >= content.end)
    {
        return true;
    }

    /*
     * 行の数を先に数えて 1 回で確保する。最後の行に行末記号がなければ 1 行足す
     */
    for (i = content.start; i < content.end; ++i)
    {
        if (data[i] == '\n')
        {
            ++count;
        }
    }
    if (data[content.end - 1] != '\n')
    {
        ++count;
    }

    if (!mnd_yaml_size_mul(count, sizeof(mnd_yaml_line_t), &bytes))
    {
        return false;
    }
    out->items = (mnd_yaml_line_t *)allocator->alloc(allocator->ctx, bytes);
    if (out->items == NULL)
    {
        lines_reset(out);
        return false;
    }
    out->cap = count;

    pos = content.start;
    for (i = 0; i < count; ++i)
    {
        pos = lines_scan_line(data, pos, content.end, first_line + i, &out->items[i]);
    }
    out->count = count;

    return true;
}

void mnd_yaml_lines_free(const mnd_yaml_allocator_t *allocator, mnd_yaml_lines_t *lines)
{
    if (lines->items != NULL)
    {
        allocator->free(allocator->ctx, lines->items);
    }
    lines_reset(lines);
}

size_t mnd_yaml_lines_index_of(const mnd_yaml_lines_t *lines, size_t offset)
{
    size_t lo = 0;
    size_t hi;

    if (lines->count == 0)
    {
        return 0;
    }

    /* start <= offset である最後の行を二分探索で探す（items[0] より前なら 0） */
    hi = lines->count;
    while ((hi - lo) > 1)
    {
        size_t mid = lo + ((hi - lo) / 2);

        if (lines->items[mid].start <= offset)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }

    return lo;
}

size_t mnd_yaml_lines_line_of(const mnd_yaml_lines_t *lines, size_t offset)
{
    const mnd_yaml_line_t *line;

    if (lines->count == 0)
    {
        return lines->first_line;
    }

    line = &lines->items[mnd_yaml_lines_index_of(lines, offset)];

    /*
     * 行末記号の LF の直後は次の行。範囲が行末記号で終わるとき、範囲の終わりは最後の行の次の行になる
     */
    if ((line->eol_len > 0) && (offset >= (line->content.end + line->eol_len)))
    {
        return line->line + 1;
    }

    return line->line;
}
