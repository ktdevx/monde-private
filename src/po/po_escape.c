/* Escape: 文字列リテラルのエスケープとアンエスケープ */

#include "po/po_internal.h"

#include "foundation/utf8.h"

#include <string.h>

/* 16 進数の数字なら値（0〜15）、そうでなければ -1 */
static int hex_value(char c)
{
    if ((c >= '0') && (c <= '9'))
    {
        return c - '0';
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        return (c - 'a') + 10;
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        return (c - 'A') + 10;
    }

    return -1;
}

/* 1 文字のエスケープ（\\ \" \n \t \r \a \b \f \v）の文字なら、対応するバイトを *byte に入れて true */
static bool simple_escape_value(char c, unsigned char *byte)
{
    switch (c)
    {
    case '\\':
        *byte = '\\';
        break;
    case '"':
        *byte = '"';
        break;
    case 'n':
        *byte = '\n';
        break;
    case 't':
        *byte = '\t';
        break;
    case 'r':
        *byte = '\r';
        break;
    case 'a':
        *byte = '\a';
        break;
    case 'b':
        *byte = '\b';
        break;
    case 'f':
        *byte = '\f';
        break;
    case 'v':
        *byte = '\v';
        break;
    default:
        return false;
    }

    return true;
}

/*
 * lit[*pos] の \ から始まるエスケープを解釈し、値を *byte に入れて *pos を直後に進める。
 * 未知のエスケープ、数字のない \x、255 を超える値は false
 */
static bool parse_escape(const char *lit, size_t len, size_t *pos, unsigned char *byte)
{
    size_t i = *pos + 1;
    unsigned int value = 0;

    if (i >= len)
    {
        return false;
    }

    if (simple_escape_value(lit[i], byte))
    {
        *pos = i + 1;
        return true;
    }

    if ((lit[i] >= '0') && (lit[i] <= '7'))
    {
        size_t end = i + 3;

        while ((i < len) && (i < end) && (lit[i] >= '0') && (lit[i] <= '7'))
        {
            value = (value * 8) + (unsigned int)(lit[i] - '0');
            i++;
        }
    }
    else if (lit[i] == 'x')
    {
        size_t digits_start = i + 1;

        i = digits_start;
        while ((i < len) && (hex_value(lit[i]) >= 0))
        {
            /* 255 を超えたら以降は値を増やさない（桁数が多くてもオーバーフローしない） */
            if (value <= 255)
            {
                value = (value * 16) + (unsigned int)hex_value(lit[i]);
            }

            i++;
        }

        if (i == digits_start)
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    if (value > 255)
    {
        return false;
    }

    *byte = (unsigned char)value;
    *pos = i;

    return true;
}

/* out の長さを start に戻す（終端の NUL も保つ） */
static void truncate_to(mnd_buf_t *out, size_t start)
{
    out->len = start;
    if (out->data != NULL)
    {
        out->data[start] = '\0';
    }
}

mnd_po_unescape_status_t mnd_po_unescape(const char *lit, size_t len, mnd_buf_t *out,
                                         size_t *bad_offset)
{
    size_t start = out->len;
    size_t i = 0;
    size_t invalid_offset;

    if ((len > 0) && !mnd_buf_reserve(out, len))
    {
        return MND_PO_UNESCAPE_NO_MEMORY;
    }

    while (i < len)
    {
        size_t run = i;

        /* エスケープでない連続したバイトはまとめて加える */
        while ((run < len) && (lit[run] != '\\'))
        {
            run++;
        }

        if (run > i)
        {
            if (!mnd_buf_append(out, lit + i, run - i))
            {
                truncate_to(out, start);
                return MND_PO_UNESCAPE_NO_MEMORY;
            }

            i = run;
        }

        if (i < len)
        {
            unsigned char byte;
            size_t backslash = i;

            if (!parse_escape(lit, len, &i, &byte))
            {
                truncate_to(out, start);
                *bad_offset = backslash;
                return MND_PO_UNESCAPE_BAD_ESCAPE;
            }

            if (!mnd_buf_append_byte(out, byte))
            {
                truncate_to(out, start);
                return MND_PO_UNESCAPE_NO_MEMORY;
            }
        }
    }

    if ((out->len > start) && (memchr(out->data + start, '\0', out->len - start) != NULL))
    {
        truncate_to(out, start);
        return MND_PO_UNESCAPE_NUL;
    }

    if ((out->len > start)
        && !mnd_utf8_validate(out->data + start, out->len - start, &invalid_offset))
    {
        truncate_to(out, start);
        return MND_PO_UNESCAPE_BAD_UTF8;
    }

    return MND_PO_UNESCAPE_OK;
}

bool mnd_po_escape(const char *s, size_t len, mnd_buf_t *out)
{
    size_t start = out->len;
    size_t i;

    for (i = 0; i < len; i++)
    {
        const char *repl = NULL;
        bool ok;

        switch (s[i])
        {
        case '\\':
            repl = "\\\\";
            break;
        case '"':
            repl = "\\\"";
            break;
        case '\n':
            repl = "\\n";
            break;
        case '\t':
            repl = "\\t";
            break;
        case '\r':
            repl = "\\r";
            break;
        case '\a':
            repl = "\\a";
            break;
        case '\b':
            repl = "\\b";
            break;
        case '\f':
            repl = "\\f";
            break;
        case '\v':
            repl = "\\v";
            break;
        default:
            break;
        }

        if (repl != NULL)
        {
            ok = mnd_buf_append(out, repl, 2);
        }
        else
        {
            ok = mnd_buf_append_byte(out, (unsigned char)s[i]);
        }

        if (!ok)
        {
            truncate_to(out, start);
            return false;
        }
    }

    return true;
}
