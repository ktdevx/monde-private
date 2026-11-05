/* Scan: 1 行の中のブロックの開始の判定（テーマ区切り、ATX、Setext の下線、フェンス、リスト記号） */

#include "markdown/md.h"
#include "markdown/md_scan.h"

/* 空白かタブ（CommonMark の space or tab）。NUL は含まない */
static bool is_space_or_tab(char c)
{
    return ((c == ' ') || (c == '\t'));
}

/* data[pos..end) が空白とタブだけ（空を含む）なら真 */
static bool rest_is_blank(const char *data, size_t pos, size_t end)
{
    size_t i = pos;

    while (i < end)
    {
        if (!is_space_or_tab(data[i]))
        {
            return false;
        }
        i++;
    }

    return true;
}

/* data[pos..end) の先頭の空白とタブを飛ばした位置 */
static size_t skip_spaces(const char *data, size_t pos, size_t end)
{
    size_t i = pos;

    while ((i < end) && is_space_or_tab(data[i]))
    {
        i++;
    }

    return i;
}

/* data[start..end) の末尾の空白とタブを除いた終わりの位置（start より前には戻らない） */
static size_t trim_end(const char *data, size_t start, size_t end)
{
    size_t i = end;

    while ((i > start) && is_space_or_tab(data[i - 1]))
    {
        i--;
    }

    return i;
}

/* data[pos..end) の先頭から続く c の数 */
static size_t count_run(const char *data, size_t pos, size_t end, char c)
{
    size_t i = pos;

    while ((i < end) && (data[i] == c))
    {
        i++;
    }

    return i - pos;
}

size_t mnd_md_scan_next_column(size_t column, char c)
{
    if (c == '\t')
    {
        return column + (4 - (column % 4));
    }

    return column + 1;
}

size_t mnd_md_scan_skip_columns(const char *data, size_t end, mnd_md_scan_pos_t *cur, size_t columns)
{
    size_t removed = 0;
    size_t width = 0;
    size_t take = 0;
    size_t physical = 0;

    /* まず、前のタブの残り（仮想の空白）を使う */
    if (cur->pad > 0)
    {
        take = (cur->pad < columns) ? cur->pad : columns;
        cur->pad -= take;
        cur->column += take;
        removed += take;
    }
    while ((removed < columns) && (cur->pos < end) && is_space_or_tab(data[cur->pos]))
    {
        /* cur->pad はここでは 0 なので、data[pos] の桁は cur->column */
        physical = cur->column;
        width = mnd_md_scan_next_column(physical, data[cur->pos]) - physical;
        take = columns - removed;
        if (width <= take)
        {
            take = width;
        }
        else
        {
            /* タブの一部だけを使う: タブを越えて進み、残りを pad にする */
            cur->pad = width - take;
        }
        cur->pos++;
        cur->column += take;
        removed += take;
    }

    return removed;
}

void mnd_md_scan_indent(const char *data, size_t end, const mnd_md_scan_pos_t *cur,
                        mnd_md_scan_indent_t *out)
{
    size_t pos = cur->pos;
    size_t column = cur->column + cur->pad;

    while ((pos < end) && is_space_or_tab(data[pos]))
    {
        column = mnd_md_scan_next_column(column, data[pos]);
        pos++;
    }
    out->pos = pos;
    out->column = column;
    out->indent = column - cur->column;
    out->blank = (pos >= end);
}

bool mnd_md_scan_thematic_break(const char *data, size_t pos, size_t end, char *ch)
{
    char c = 0;
    size_t count = 0;
    size_t i = pos;

    if (pos >= end)
    {
        return false;
    }
    c = data[pos];
    if ((c != '*') && (c != '-') && (c != '_'))
    {
        return false;
    }
    while (i < end)
    {
        if (data[i] == c)
        {
            count++;
        }
        else if (!is_space_or_tab(data[i]))
        {
            return false;
        }
        i++;
    }
    if (count < 3)
    {
        return false;
    }
    if (ch != NULL)
    {
        *ch = c;
    }

    return true;
}

bool mnd_md_scan_atx_heading(const char *data, size_t pos, size_t end, mnd_md_heading_t *out)
{
    size_t level = count_run(data, pos, end, '#');
    size_t open_end = pos + level;
    size_t start = 0;
    size_t stop = 0;
    size_t hashes = 0;

    if ((level < 1) || (level > 6))
    {
        return false;
    }
    if ((open_end < end) && !is_space_or_tab(data[open_end]))
    {
        return false;
    }

    /* 開きの並びの後ろの空白と、行末の空白を除く */
    start = skip_spaces(data, open_end, end);
    stop = trim_end(data, start, end);

    /* 閉じの並び: 末尾の # の並びで、前が空白かタブ（または内容の先頭） */
    hashes = stop;
    while ((hashes > start) && (data[hashes - 1] == '#'))
    {
        hashes--;
    }
    if (hashes < stop)
    {
        if (hashes == start)
        {
            stop = start;
        }
        else if (is_space_or_tab(data[hashes - 1]))
        {
            stop = trim_end(data, start, hashes);
        }
    }

    if (start == stop)
    {
        /* 空の内容は開きの並びの直後の空の範囲 */
        start = open_end;
        stop = open_end;
    }
    out->level = (int)level;
    out->setext = false;
    out->content.start = start;
    out->content.end = stop;

    return true;
}

int mnd_md_scan_setext_underline(const char *data, size_t pos, size_t end)
{
    size_t count = 0;
    char c = 0;

    if (pos >= end)
    {
        return 0;
    }
    c = data[pos];
    if ((c != '=') && (c != '-'))
    {
        return 0;
    }
    count = count_run(data, pos, end, c);
    if (!rest_is_blank(data, pos + count, end))
    {
        return 0;
    }

    return (c == '=') ? 1 : 2;
}

bool mnd_md_scan_open_fence(const char *data, size_t pos, size_t end, mnd_md_code_block_t *out)
{
    char c = 0;
    size_t len = 0;
    size_t info_start = 0;
    size_t info_end = 0;
    size_t i = 0;

    if (pos >= end)
    {
        return false;
    }
    c = data[pos];
    if ((c != '`') && (c != '~'))
    {
        return false;
    }
    len = count_run(data, pos, end, c);
    if (len < 3)
    {
        return false;
    }
    if (c == '`')
    {
        /* バッククォートのフェンスの情報文字列はバッククォートを含まない */
        for (i = pos + len; i < end; i++)
        {
            if (data[i] == '`')
            {
                return false;
            }
        }
    }
    info_start = skip_spaces(data, pos + len, end);
    info_end = trim_end(data, info_start, end);

    out->fenced = true;
    out->fence_char = c;
    out->fence_len = len;
    out->fence_indent = 0;
    out->has_info = (info_start < info_end);
    if (out->has_info)
    {
        out->info.start = info_start;
        out->info.end = info_end;
    }
    else
    {
        out->info.start = pos + len;
        out->info.end = pos + len;
    }
    out->closed = false;

    return true;
}

bool mnd_md_scan_close_fence(const char *data, size_t pos, size_t end, char fence_char,
                             size_t fence_len)
{
    size_t len = 0;

    if ((pos >= end) || (data[pos] != fence_char))
    {
        return false;
    }
    len = count_run(data, pos, end, fence_char);
    if (len < fence_len)
    {
        return false;
    }

    return rest_is_blank(data, pos + len, end);
}

bool mnd_md_scan_list_marker(const char *data, size_t pos, size_t end, mnd_md_scan_list_marker_t *out)
{
    size_t i = pos;
    unsigned long start = 0;
    bool ordered = false;
    char marker = 0;

    if (pos >= end)
    {
        return false;
    }
    if ((data[i] == '-') || (data[i] == '+') || (data[i] == '*'))
    {
        marker = data[i];
        i++;
    }
    else
    {
        /* 1〜9 桁の数字と `.` か `)`。9 桁までなので unsigned long に収まる */
        while ((i < end) && (data[i] >= '0') && (data[i] <= '9'))
        {
            if ((i - pos) >= 9)
            {
                return false;
            }
            start = (start * 10UL) + (unsigned long)(data[i] - '0');
            i++;
        }
        if ((i == pos) || (i >= end) || ((data[i] != '.') && (data[i] != ')')))
        {
            return false;
        }
        ordered = true;
        marker = data[i];
        i++;
    }
    if ((i < end) && !is_space_or_tab(data[i]))
    {
        return false;
    }

    out->ordered = ordered;
    out->marker = marker;
    out->start = start;
    out->span.start = pos;
    out->span.end = i;
    out->rest_blank = rest_is_blank(data, i, end);

    return true;
}

size_t mnd_md_scan_list_padding(const char *data, size_t pos, size_t end, size_t column)
{
    mnd_md_scan_pos_t cur;
    mnd_md_scan_indent_t ind;

    cur.pos = pos;
    cur.column = column;
    cur.pad = 0;
    mnd_md_scan_indent(data, end, &cur, &ind);
    if (ind.blank || (ind.indent >= 5) || (ind.indent == 0))
    {
        return 1;
    }

    return ind.indent;
}
