/* Text: mnd_md_logical_text、mnd_md_unescape */

#include "markdown/md.h"
#include "markdown/md_entity.h"
#include "markdown/md_internal.h"

#include <stdbool.h>
#include <stddef.h>

/* 数値文字参照の 10 進の最大の桁数と、16 進の最大の桁数（spec 0.31.2） */
#define MD_TEXT_DEC_DIGITS_MAX 7
#define MD_TEXT_HEX_DIGITS_MAX 6

/* 置換文字 U+FFFD */
#define MD_TEXT_REPLACEMENT 0xFFFDUL

static size_t min_size(size_t a, size_t b)
{
    return (a < b) ? a : b;
}

static size_t max_size(size_t a, size_t b)
{
    return (a > b) ? a : b;
}

/* count 個の空白を追加する */
static bool append_spaces(mnd_buf_t *out, size_t count)
{
    static const char spaces[] = "    ";
    size_t n;

    while (count > 0)
    {
        n = min_size(count, sizeof(spaces) - 1);
        if (!mnd_buf_append(out, spaces, n))
        {
            return false;
        }

        count -= n;
    }

    return true;
}

/*
 * 行の内容 [cs, ce) が範囲 [start, end) に含まれるか（内容の一部と補う空白が論理的な文字列に現れるか）。
 * 内容と 1 バイト以上重なる行は含まれる。内容が空の行は、その位置が範囲の内側か端（start <= cs <= end）
 * なら含まれる（リーフの範囲は最後の行の line_end で終わるので、最後の空の行も含まれる）。
 * 空でない内容と、境目でだけ接する行は含まれない（境目の "\n" は boundary_in_span が決める）
 */
static bool line_in_span(const mnd_md_line_t *line, size_t start, size_t end)
{
    size_t cs = line->content.start;
    size_t ce = line->content.end;

    if (cs == ce)
    {
        return ((start <= cs) && (cs <= end));
    }

    return (max_size(start, cs) < min_size(end, ce));
}

/*
 * 行 line と次の行 next の境目が範囲 [start, end) の内側にあるか。範囲が line の内容の終わりから
 * next の内容の始まりまでを覆う（start <= line の content.end、next の content.start <= end）なら内側で、
 * 論理的な文字列に "\n" が現れる。境目のプレフィックスと行末記号のバイト自体は文字列に現れない
 */
static bool boundary_in_span(const mnd_md_line_t *line, const mnd_md_line_t *next, size_t start, size_t end)
{
    return ((start <= line->content.end) && (next->content.start <= end));
}

bool mnd_md_logical_text(const mnd_md_doc_t *doc, const mnd_md_node_t *leaf, mnd_span_t span,
                         mnd_buf_t *out)
{
    const mnd_md_line_t *line;
    size_t start;
    size_t end;
    size_t from;
    size_t to;
    size_t i;

    if ((doc == NULL) || (doc->data == NULL))
    {
        return true;
    }

    end = min_size(span.end, doc->len);
    start = min_size(span.start, end);

    if ((leaf == NULL) || (leaf->lines == NULL) || (leaf->line_count == 0))
    {
        return mnd_buf_append(out, doc->data + start, end - start);
    }

    for (i = 0; i < leaf->line_count; i++)
    {
        line = &leaf->lines[i];
        if (line_in_span(line, start, end))
        {
            /* 補う空白は内容の先頭の前にあるので、範囲が内容の先頭を含むときだけ加える */
            if ((start <= line->content.start) && !append_spaces(out, line->pad))
            {
                return false;
            }

            from = max_size(start, line->content.start);
            to = min_size(end, line->content.end);
            if ((from < to) && !mnd_buf_append(out, doc->data + from, to - from))
            {
                return false;
            }
        }

        /* 範囲の内側にある行の境目ごとに "\n" を加える（範囲が境目で始まるか終わる場合も含む） */
        if (((i + 1) < leaf->line_count) && boundary_in_span(line, &leaf->lines[i + 1], start, end)
            && !mnd_buf_append_byte(out, '\n'))
        {
            return false;
        }
    }

    return true;
}

static bool is_ascii_punct(unsigned char c)
{
    return (((c >= 0x21) && (c <= 0x2F)) || ((c >= 0x3A) && (c <= 0x40))
            || ((c >= 0x5B) && (c <= 0x60)) || ((c >= 0x7B) && (c <= 0x7E)));
}

static bool is_ascii_alnum(unsigned char c)
{
    return (((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')));
}

/* 16 進の数字の値。数字でなければ -1 */
static int hex_value(unsigned char c)
{
    if ((c >= '0') && (c <= '9'))
    {
        return c - '0';
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        return c - 'a' + 10;
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        return c - 'A' + 10;
    }

    return -1;
}

/* コードポイント cp を UTF-8 で追加する。0、サロゲート、U+10FFFF を超える値は U+FFFD にする */
static bool append_codepoint(mnd_buf_t *out, unsigned long cp)
{
    unsigned char bytes[4];
    size_t n;

    if ((cp == 0) || ((cp >= 0xD800UL) && (cp <= 0xDFFFUL)) || (cp > 0x10FFFFUL))
    {
        cp = MD_TEXT_REPLACEMENT;
    }

    if (cp < 0x80UL)
    {
        bytes[0] = (unsigned char)cp;
        n = 1;
    }
    else if (cp < 0x800UL)
    {
        bytes[0] = (unsigned char)(0xC0UL | (cp >> 6));
        bytes[1] = (unsigned char)(0x80UL | (cp & 0x3FUL));
        n = 2;
    }
    else if (cp < 0x10000UL)
    {
        bytes[0] = (unsigned char)(0xE0UL | (cp >> 12));
        bytes[1] = (unsigned char)(0x80UL | ((cp >> 6) & 0x3FUL));
        bytes[2] = (unsigned char)(0x80UL | (cp & 0x3FUL));
        n = 3;
    }
    else
    {
        bytes[0] = (unsigned char)(0xF0UL | (cp >> 18));
        bytes[1] = (unsigned char)(0x80UL | ((cp >> 12) & 0x3FUL));
        bytes[2] = (unsigned char)(0x80UL | ((cp >> 6) & 0x3FUL));
        bytes[3] = (unsigned char)(0x80UL | (cp & 0x3FUL));
        n = 4;
    }

    return mnd_buf_append(out, bytes, n);
}

/*
 * text[pos] == '&' から始まる数値文字参照（`&#d{1,7};`、`&#[xX]h{1,6};`）を認識する。
 * 認識したら値を *cp に、`;` の後ろの位置を *next に入れて true を返す
 */
static bool scan_numeric_ref(const unsigned char *text, size_t pos, size_t len, unsigned long *cp,
                             size_t *next)
{
    unsigned long value = 0;
    size_t p = pos + 2;
    size_t digits = 0;
    int digit;

    if (((pos + 2) >= len) || (text[pos + 1] != '#'))
    {
        return false;
    }

    if ((text[p] == 'x') || (text[p] == 'X'))
    {
        p++;
        while ((p < len) && (digits <= MD_TEXT_HEX_DIGITS_MAX) && ((digit = hex_value(text[p])) >= 0))
        {
            value = (value * 16UL) + (unsigned long)digit;
            digits++;
            p++;
        }

        if ((digits == 0) || (digits > MD_TEXT_HEX_DIGITS_MAX))
        {
            return false;
        }
    }
    else
    {
        while ((p < len) && (digits <= MD_TEXT_DEC_DIGITS_MAX) && (text[p] >= '0') && (text[p] <= '9'))
        {
            value = (value * 10UL) + (unsigned long)(text[p] - '0');
            digits++;
            p++;
        }

        if ((digits == 0) || (digits > MD_TEXT_DEC_DIGITS_MAX))
        {
            return false;
        }
    }

    if ((p >= len) || (text[p] != ';'))
    {
        return false;
    }

    *cp = value;
    *next = p + 1;

    return true;
}

/*
 * text[pos] == '&' から始まる実体参照 `&name;` を認識する。表にあれば、表す列を *utf8、*utf8_len に、
 * `;` の後ろの位置を *next に入れて true を返す
 */
static bool scan_entity_ref(const unsigned char *text, size_t pos, size_t len, const unsigned char **utf8,
                            size_t *utf8_len, size_t *next)
{
    size_t p = pos + 1;

    while ((p < len) && ((p - (pos + 1)) <= MND_MD_ENTITY_NAME_MAX) && is_ascii_alnum(text[p]))
    {
        p++;
    }

    if ((p >= len) || (text[p] != ';'))
    {
        return false;
    }

    if (!mnd_md_entity_lookup((const char *)text + pos + 1, p - (pos + 1), utf8, utf8_len))
    {
        return false;
    }

    *next = p + 1;

    return true;
}

bool mnd_md_unescape(const char *text, size_t len, mnd_buf_t *out)
{
    const unsigned char *s = (const unsigned char *)text;
    const unsigned char *utf8;
    size_t utf8_len;
    unsigned long cp;
    size_t run = 0;  /* そのまま写すバイトの並びの開始 */
    size_t pos = 0;
    size_t next;

    if (len == 0)
    {
        return true;
    }

    while (pos < len)
    {
        if ((s[pos] == '\\') && ((pos + 1) < len) && is_ascii_punct(s[pos + 1]))
        {
            if (!mnd_buf_append(out, s + run, pos - run) || !mnd_buf_append_byte(out, s[pos + 1]))
            {
                return false;
            }

            pos += 2;
            run = pos;
        }
        else if ((s[pos] == '&') && scan_numeric_ref(s, pos, len, &cp, &next))
        {
            if (!mnd_buf_append(out, s + run, pos - run) || !append_codepoint(out, cp))
            {
                return false;
            }

            pos = next;
            run = pos;
        }
        else if ((s[pos] == '&') && scan_entity_ref(s, pos, len, &utf8, &utf8_len, &next))
        {
            if (!mnd_buf_append(out, s + run, pos - run) || !mnd_buf_append(out, utf8, utf8_len))
            {
                return false;
            }

            pos = next;
            run = pos;
        }
        else
        {
            pos++;
        }
    }

    return mnd_buf_append(out, s + run, pos - run);
}
