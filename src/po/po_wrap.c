/* Wrap: 文字列 1 つを PO の行の列に整形する */

#include "po/po_internal.h"

#include <string.h>

/* 行の幅（引用符と前置きを含む桁数の上限） */
#define MND_PO_WRAP_WIDTH 79

/*
 * s[i] から始まる UTF-8 の 1 文字を *cp に入れ、そのバイト数を返す。
 * 呼び出し側は正しい UTF-8 を渡す。途中で切れたシーケンスは 1 バイトの文字として扱う
 */
static size_t decode_char(const char *s, size_t len, size_t i, uint32_t *cp)
{
    unsigned char lead = (unsigned char)s[i];
    size_t count;
    uint32_t value;
    size_t k;

    if (lead < 0x80)
    {
        *cp = lead;

        return 1;
    }

    if ((lead & 0xE0) == 0xC0)
    {
        count = 2;
        value = lead & 0x1F;
    }
    else if ((lead & 0xF0) == 0xE0)
    {
        count = 3;
        value = lead & 0x0F;
    }
    else
    {
        count = 4;
        value = lead & 0x07;
    }

    if (count > (len - i))
    {
        *cp = lead;

        return 1;
    }

    for (k = 1; k < count; k++)
    {
        value = (value << 6) | ((unsigned char)s[i + k] & 0x3F);
    }

    *cp = value;

    return count;
}

/* s[0..len) の桁数（Ucd の文字幅の合計） */
static size_t columns(const char *s, size_t len)
{
    size_t total = 0;
    size_t i = 0;

    while (i < len)
    {
        uint32_t cp;

        i += decode_char(s, len, i, &cp);
        total += (size_t)mnd_po_ucd_width(cp);
    }

    return total;
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

/* 続きの行 prefix + '"' + part[0..len) + '"' + eol を out に加える */
static bool append_line(const char *prefix, const char *part, size_t len, const char *eol,
                        mnd_buf_t *out)
{
    return mnd_buf_append_str(out, prefix) && mnd_buf_append_byte(out, '"')
        && mnd_buf_append(out, part, len) && mnd_buf_append_byte(out, '"')
        && mnd_buf_append_str(out, eol);
}

/* s[i] の直後で終わる区分の終わりの位置（\n の直後、または len） */
static size_t segment_end(const char *s, size_t len, size_t i)
{
    const char *nl = (i < len) ? memchr(s + i, '\n', len - i) : NULL;

    return (nl != NULL) ? (size_t)((nl - s) + 1) : len;
}

/*
 * 区分 seg[0..len) をエスケープして esc に入れ、ebreaks に「エスケープ後のバイト位置の直前で
 * 区切れるなら 1」を入れる。gettext（msgcat 0.21）と同じく、分割の機会はエスケープ後の文字列で
 * 求め、エスケープの内部（\ の次の文字の前）と、\n のエスケープの直前では区切らない
 */
static bool escape_segment(const char *seg, size_t len, mnd_buf_t *esc, mnd_buf_t *ebreaks)
{
    size_t i;

    mnd_buf_clear(esc);
    mnd_buf_clear(ebreaks);
    if (len == 0)
    {
        return true;
    }

    if (!mnd_po_escape(seg, len, esc) || !mnd_buf_reserve(ebreaks, esc->len))
    {
        return false;
    }

    mnd_po_linebreaks(esc->data, esc->len, (unsigned char *)ebreaks->data);
    ebreaks->len = esc->len;
    ebreaks->data[ebreaks->len] = '\0';
    /* エスケープ後の \ は、必ず 2 文字のエスケープの先頭である */
    for (i = 0; (i + 1) < esc->len; i++)
    {
        if (esc->data[i] == '\\')
        {
            if (esc->data[i + 1] == 'n')
            {
                ebreaks->data[i] = 0;
            }

            ebreaks->data[i + 1] = 0;
            i++;
        }
    }

    return true;
}

/* 区分の中に分割の機会があるか */
static bool has_break_opportunity(const mnd_buf_t *ebreaks)
{
    return (ebreaks->len > 0) && (memchr(ebreaks->data, 1, ebreaks->len) != NULL);
}

/*
 * エスケープ済みの区分 esc[0..len) を、分割の機会の位置で貪欲に続きの行に分けて書く。
 * 各行は桁(prefix + '"' + 部分 + '"') <= W を満たす最も長い部分とする。最初の分割の機会までで
 * W を超える場合は、その位置までを 1 行にする
 */
static bool append_wrapped(const char *prefix, const char *esc, const unsigned char *ebreaks,
                           size_t len, const char *eol, mnd_buf_t *out)
{
    size_t base = columns(prefix, strlen(prefix)) + 1;
    size_t line_start = 0;
    size_t last_break = 0;
    bool has_break = false;
    size_t col = base;
    size_t piece = 0;
    size_t i = 0;

    while (i < len)
    {
        uint32_t cp;

        if (ebreaks[i] != 0)
        {
            if (has_break && ((col + piece + 1) > MND_PO_WRAP_WIDTH))
            {
                if (!append_line(prefix, esc + line_start, last_break - line_start, eol, out))
                {
                    return false;
                }

                line_start = last_break;
                col = base;
            }

            col += piece;
            piece = 0;
            last_break = i;
            has_break = true;
        }

        i += decode_char(esc, len, i, &cp);
        piece += (size_t)mnd_po_ucd_width(cp);
    }

    if (has_break && ((col + piece + 1) > MND_PO_WRAP_WIDTH))
    {
        if (!append_line(prefix, esc + line_start, last_break - line_start, eol, out))
        {
            return false;
        }

        line_start = last_break;
    }

    return append_line(prefix, esc + line_start, len - line_start, eol, out);
}

bool mnd_po_wrap_string(const char *prefix, const char *keyword, const char *s, size_t len,
                        bool wrap, const char *eol, mnd_buf_t *out)
{
    size_t start = out->len;
    mnd_buf_t esc = MND_BUF_INIT;
    mnd_buf_t ebreaks = MND_BUF_INIT;
    size_t first_end = segment_end(s, len, 0);
    bool ok;

    /*
     * 区分が 1 つ（途中に \n がない）なら、1 行に収まるか、分割の機会がないなら（W を超えても）
     * キーワードの行に書く（gettext と同じく、実際に区切るときだけ "" の行にする）
     */
    ok = escape_segment(s, first_end, &esc, &ebreaks);
    if (ok && (first_end == len))
    {
        size_t width = columns(prefix, strlen(prefix)) + columns(keyword, strlen(keyword)) + 2
                     + columns(esc.data, esc.len) + 1;

        if (!wrap || (width <= MND_PO_WRAP_WIDTH) || !has_break_opportunity(&ebreaks))
        {
            ok = mnd_buf_append_str(out, prefix) && mnd_buf_append_str(out, keyword)
              && mnd_buf_append_str(out, " \"") && mnd_buf_append(out, esc.data, esc.len)
              && mnd_buf_append_byte(out, '"') && mnd_buf_append_str(out, eol);
            mnd_buf_free(&esc);
            mnd_buf_free(&ebreaks);
            if (!ok)
            {
                truncate_to(out, start);
            }

            return ok;
        }
    }

    /* 複数行: キーワードの行に "" を書き、各区分を続きの行に書く。末尾の空の区分は捨てる */
    ok = ok && mnd_buf_append_str(out, prefix) && mnd_buf_append_str(out, keyword)
      && mnd_buf_append_str(out, " \"\"") && mnd_buf_append_str(out, eol);
    if (ok)
    {
        size_t seg_start = 0;
        size_t seg_end = first_end;

        for (;;)
        {
            if (!wrap)
            {
                ok = append_line(prefix, esc.data, esc.len, eol, out);
            }
            else
            {
                ok = append_wrapped(prefix, esc.data, (const unsigned char *)ebreaks.data, esc.len,
                                    eol, out);
            }

            if (!ok || (seg_end >= len))
            {
                break;
            }

            seg_start = seg_end;
            seg_end = segment_end(s, len, seg_start);
            ok = escape_segment(s + seg_start, seg_end - seg_start, &esc, &ebreaks);
            if (!ok)
            {
                break;
            }
        }
    }

    mnd_buf_free(&esc);
    mnd_buf_free(&ebreaks);

    if (!ok)
    {
        truncate_to(out, start);
    }

    return ok;
}
