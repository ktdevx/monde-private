/* StructureCheck: 構造を変え得る訳文の検査 */

#include "tu/tu_internal.h"

#include <string.h>

/* 空白（スペース）かタブか */
static bool check_is_blank(char c)
{
    return (c == ' ') || (c == '\t');
}

/* 数字か */
static bool check_is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

/* line[pos] が行末か、空白・タブか（記号の後ろの条件） */
static bool check_ends_marker(const char *line, size_t len, size_t pos)
{
    return (pos >= len) || check_is_blank(line[pos]);
}

/* line[0..len) が空、または空白・タブだけか */
static bool check_is_blank_line(const char *line, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++)
    {
        if (!check_is_blank(line[i]))
        {
            return false;
        }
    }
    return true;
}

/* line[pos..len) が同じ記号 marker の 3 個以上で、ほかは空白・タブだけか（区切り線） */
static bool check_is_thematic_break(const char *line, size_t len, size_t pos, char marker)
{
    size_t count = 0;
    size_t i;

    for (i = pos; i < len; i++)
    {
        if (line[i] == marker)
        {
            count++;
        }
        else if (!check_is_blank(line[i]))
        {
            return false;
        }
    }
    return count >= 3;
}

/*
 * line[0..len)（行末記号を含まない）が、先頭の 0〜3 個の空白のあとに
 * ブロックを開始し得る記号で始まるか
 */
static bool check_starts_block(const char *line, size_t len)
{
    size_t pos = 0;
    size_t run;
    char c;

    while ((pos < len) && (pos < 3) && (line[pos] == ' '))
    {
        pos++;
    }
    if (pos >= len)
    {
        return false;
    }
    c = line[pos];

    if (c == '#')
    {
        /* ATX 見出し: # 1〜6 個の後ろが空白か行末 */
        run = 0;
        while (((pos + run) < len) && (line[pos + run] == '#'))
        {
            run++;
        }
        return (run <= 6) && check_ends_marker(line, len, pos + run);
    }
    if (c == '>')
    {
        /* 引用 */
        return true;
    }
    if ((c == '-') || (c == '*') || (c == '_'))
    {
        /* 区切り線（同じ記号の 3 個以上） */
        if (check_is_thematic_break(line, len, pos, c))
        {
            return true;
        }
    }
    if ((c == '-') || (c == '+') || (c == '*'))
    {
        /* 箇条書き: 記号の後ろが空白か行末 */
        return check_ends_marker(line, len, pos + 1);
    }
    if (check_is_digit(c))
    {
        /* 番号付き: 1〜9 桁の数字と . / ) の後ろが空白か行末 */
        run = 0;
        while (((pos + run) < len) && check_is_digit(line[pos + run]))
        {
            run++;
        }
        if ((run > 9) || ((pos + run) >= len))
        {
            return false;
        }
        if ((line[pos + run] != '.') && (line[pos + run] != ')'))
        {
            return false;
        }
        return check_ends_marker(line, len, pos + run + 1);
    }
    if ((c == '`') || (c == '~'))
    {
        /* フェンス: 同じ記号の 3 個以上（後ろは問わない） */
        run = 0;
        while (((pos + run) < len) && (line[pos + run] == c))
        {
            run++;
        }
        return run >= 3;
    }
    if (c == '=')
    {
        /* setext の下線: = だけ 1 個以上（末尾の空白・タブは許す） */
        run = 0;
        while (((pos + run) < len) && (line[pos + run] == '='))
        {
            run++;
        }
        return check_is_blank_line(line + pos + run, len - (pos + run));
    }
    return false;
}

/*
 * 表の項目を表の順（空行、1 行の見出し、ブロックの開始）に評価し、最初に当てはまった項目を返す。
 * 行は \n で区切る（訳文の論理テキストの行の境界）。末尾の \n のあとと空の text は空の行として数える
 */
mnd_tu_check_result_t mnd_tu_check_structure(const char *text, size_t len, bool is_atx_heading,
                                             size_t *line_index)
{
    size_t pos;
    size_t index;

    /* 空行 */
    pos = 0;
    for (;;)
    {
        const char *nl = (pos < len) ? (const char *)memchr(text + pos, '\n', len - pos) : NULL;
        size_t end = (nl != NULL) ? (size_t)(nl - text) : len;

        if (check_is_blank_line(text + pos, end - pos))
        {
            return MND_TU_CHECK_BLANK_LINE;
        }
        if (nl == NULL)
        {
            break;
        }
        pos = end + 1;
    }

    /* 1 行の見出し */
    if (is_atx_heading && (len > 0) && (memchr(text, '\n', len) != NULL))
    {
        return MND_TU_CHECK_HEADING_LINE_BREAK;
    }

    /* ブロックの開始 */
    pos = 0;
    index = 0;
    for (;;)
    {
        const char *nl = (pos < len) ? (const char *)memchr(text + pos, '\n', len - pos) : NULL;
        size_t end = (nl != NULL) ? (size_t)(nl - text) : len;

        if (check_starts_block(text + pos, end - pos))
        {
            *line_index = index;
            return MND_TU_CHECK_BLOCK_MARKER;
        }
        if (nl == NULL)
        {
            break;
        }
        pos = end + 1;
        index++;
    }
    return MND_TU_CHECK_OK;
}
