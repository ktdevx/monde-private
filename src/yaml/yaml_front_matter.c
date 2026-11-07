/* FrontMatter: 開きと閉じの行の検出、本文の開始位置 */

#include "yaml/yaml_front_matter.h"

#include "foundation/utf8.h"

#include <stddef.h>

static void set_absent(mnd_yaml_front_matter_t *out)
{
    out->present = false;
    out->span.start = 0;
    out->span.end = 0;
    out->content.start = 0;
    out->content.end = 0;
    out->body_start = 0;
}

/*
 * pos から始まる行の内容の終わり（行末記号の前）を *content_end に、次の行の先頭を *next に入れる。
 * 行末記号は LF と CRLF。単独の CR は行の内容に含める。行末記号がなければ両方とも len
 */
static void scan_line(const char *data, size_t len, size_t pos, size_t *content_end, size_t *next)
{
    size_t i = pos;

    while ((i < len) && (data[i] != '\n'))
    {
        i++;
    }

    if (i < len)
    {
        *next = i + 1;
        if ((i > pos) && (data[i - 1] == '\r'))
        {
            *content_end = i - 1;
        }
        else
        {
            *content_end = i;
        }
    }
    else
    {
        *next = len;
        *content_end = len;
    }
}

/*
 * data[start..end) が mark の 3 文字と、その後ろの空白・タブだけからなるなら true
 */
static bool is_marker_line(const char *data, size_t start, size_t end, char mark)
{
    size_t i;

    if ((end - start) < 3)
    {
        return false;
    }

    if ((data[start] != mark) || (data[start + 1] != mark) || (data[start + 2] != mark))
    {
        return false;
    }

    for (i = start + 3; i < end; i++)
    {
        if ((data[i] != ' ') && (data[i] != '\t'))
        {
            return false;
        }
    }

    return true;
}

bool mnd_yaml_front_matter_detect(const char *data, size_t len, const char *path,
                                  mnd_yaml_front_matter_t *out, mnd_diag_list_t *diags)
{
    size_t invalid_offset = 0;
    size_t line = 0;
    size_t content_end;
    size_t pos;
    size_t next;

    set_absent(out);

    /* 文書全体の UTF-8 を検証する（6.6）。行番号は文書全体の先頭から数える（7.1） */
    if (!mnd_utf8_validate(data, len, &invalid_offset))
    {
        if (!mnd_source_line_of(data, len, invalid_offset, &line))
        {
            line = 0;
        }

        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, line, "invalid UTF-8 sequence");
        return false;
    }

    if (len == 0)
    {
        return true;
    }

    /* 1 行目が開きの行でなければフロントマターなし（1.3） */
    scan_line(data, len, 0, &content_end, &next);
    if (!is_marker_line(data, 0, content_end, '-'))
    {
        return true;
    }

    /* 2 行目以降で最初の閉じの行を探す（1.1）。なければフロントマターなし（1.4） */
    pos = next;
    while (pos < len)
    {
        size_t line_end;
        size_t line_next;

        scan_line(data, len, pos, &line_end, &line_next);
        if (is_marker_line(data, pos, line_end, '-') || is_marker_line(data, pos, line_end, '.'))
        {
            out->present = true;
            out->span.start = 0;
            out->span.end = line_next;
            out->content.start = next;
            out->content.end = pos;
            out->body_start = line_next;
            return true;
        }

        pos = line_next;
    }

    return true;
}
