/* Writer: エントリの各部分の並べ方、コメント・参照・フラグの行 */

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"
#include "po/po_internal.h"
#include "po/po_write.h"

#include <string.h>

/* 参照の行の最大のバイト数 */
#define MND_PO_WRITE_WIDTH 79
/* 診断に表示する値の最大のバイト数 */
#define MND_PO_WRITE_SHOW_MAX 40

/* 書き出しの設定（エントリごとの折り返しの有無は別に決める） */
typedef struct mnd_po_writer
{
    const char *eol;
    bool no_wrap;
    mnd_buf_t *out;
} mnd_po_writer_t;

/* UTF-8 の先頭バイトから文字のバイト数を返す（正しい UTF-8 を前提とする） */
static size_t mnd_po_write_char_len(unsigned char lead)
{
    if (lead < 0x80)
    {
        return 1;
    }
    else if (lead < 0xE0)
    {
        return 2;
    }
    else if (lead < 0xF0)
    {
        return 3;
    }
    else
    {
        return 4;
    }
}

/*
 * 診断に表示する値 "<値>" を show に加える。文字の途中で切らずに MND_PO_WRITE_SHOW_MAX バイトまでとし、
 * 残りがあれば ... を付ける。制御文字（C0、DEL、C1）は '?' にする
 */
static bool mnd_po_write_show(const mnd_buf_t *value, mnd_buf_t *show)
{
    const char *s = mnd_buf_cstr(value);
    size_t len = value->len;
    size_t i = 0;
    size_t n = 0;

    if (!mnd_buf_append_byte(show, '"'))
    {
        return false;
    }

    while (i < len)
    {
        unsigned char c = (unsigned char)s[i];
        size_t char_len = mnd_po_write_char_len(c);
        bool control = (c < 0x20) || (c == 0x7F)
                       || ((c == 0xC2) && (i + 1 < len) && ((unsigned char)s[i + 1] < 0xA0));
        bool ok;

        if (char_len > (len - i))
        {
            char_len = len - i;
        }

        if ((n + (control ? 1 : char_len)) > MND_PO_WRITE_SHOW_MAX)
        {
            break;
        }

        if (control)
        {
            ok = mnd_buf_append_byte(show, '?');
            n++;
        }
        else
        {
            ok = mnd_buf_append(show, s + i, char_len);
            n += char_len;
        }

        if (!ok)
        {
            return false;
        }

        i += char_len;
    }

    if ((i < len) && !mnd_buf_append_str(show, "..."))
    {
        return false;
    }

    return mnd_buf_append_byte(show, '"');
}

/* 重複の組をすべて報告する。重複があれば true（メモリ不足でも、重複があれば true） */
static bool mnd_po_write_report_duplicates(const mnd_po_catalog_t *catalog,
                                           mnd_diag_list_t *diags)
{
    size_t from = 0;
    size_t first;
    size_t second;
    bool found = false;
    mnd_buf_t show = MND_BUF_INIT;

    while (mnd_po_find_duplicate(catalog, from, &first, &second))
    {
        const mnd_po_entry_t *entry = &catalog->entries[second];
        bool ok = true;

        (void)first;
        found = true;
        mnd_buf_clear(&show);
        if (entry->has_msgctxt)
        {
            ok = mnd_po_write_show(&entry->msgctxt, &show);
        }
        else
        {
            ok = mnd_buf_append_str(&show, "(none)");
        }

        ok = ok && mnd_buf_append_str(&show, ", msgid ") && mnd_po_write_show(&entry->msgid, &show);
        if (ok)
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                         "duplicate message definition: msgctxt %s", mnd_buf_cstr(&show));
        }
        else
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory");
        }

        from = second + 1;
    }

    mnd_buf_free(&show);

    return found;
}

/* "<mark><text>"（text が空なら "<mark>" だけ）の行をコメントの数だけ書く */
static bool mnd_po_write_comments(mnd_po_writer_t *writer, const char *mark,
                                  const mnd_po_strings_t *comments)
{
    size_t i;

    for (i = 0; i < comments->count; i++)
    {
        const mnd_buf_t *text = &comments->items[i];

        if (!mnd_buf_append_str(writer->out, mark))
        {
            return false;
        }

        if ((text->len > 0)
            && (!mnd_buf_append_byte(writer->out, ' ')
                || !mnd_buf_append(writer->out, text->data, text->len)))
        {
            return false;
        }

        if (!mnd_buf_append_str(writer->out, writer->eol))
        {
            return false;
        }
    }

    return true;
}

/*
 * ファイル名を U+2068 と U+2069 で囲むか。空白かタブを含む場合と、行番号もない空のファイル名の
 * 場合（何も書かないと読み直したときに参照が消えるため）に囲む
 */
static bool mnd_po_write_needs_isolate(const mnd_po_ref_t *ref)
{
    const mnd_buf_t *file = &ref->file;
    size_t i;

    if ((file->len == 0) && (ref->line == 0))
    {
        return true;
    }

    for (i = 0; i < file->len; i++)
    {
        if ((file->data[i] == ' ') || (file->data[i] == '\t'))
        {
            return true;
        }
    }

    return false;
}

/*
 * 参照の行を書く。行に参照が 1 つ以上あり、" <ref>" を加えると行のバイト数が 79 を超える場合は、
 * 新しい #: の行を始める。msgcat 0.21 の --no-wrap と同じく、no_wrap でも折り返す
 */
static bool mnd_po_write_refs(mnd_po_writer_t *writer, const mnd_po_refs_t *refs)
{
    mnd_buf_t *out = writer->out;
    size_t line_start;
    size_t i;

    if (refs->count == 0)
    {
        return true;
    }

    line_start = out->len;
    if (!mnd_buf_append_str(out, "#:"))
    {
        return false;
    }

    for (i = 0; i < refs->count; i++)
    {
        const mnd_po_ref_t *ref = &refs->items[i];
        bool isolate = mnd_po_write_needs_isolate(ref);
        char number[32];
        size_t number_len = 0;
        size_t piece_len;
        size_t line_len;

        if (ref->line > 0)
        {
            size_t digits = mnd_po_format_decimal(ref->line, 0, number + 1, sizeof(number) - 1);

            if (digits == 0)
            {
                return false;
            }

            number[0] = ':';
            number_len = 1 + digits;
        }

        piece_len = 1 + ref->file.len + (isolate ? 6 : 0) + number_len;
        line_len = out->len - line_start;
        if ((line_len > 2) && ((line_len + piece_len) > MND_PO_WRITE_WIDTH))
        {
            if (!mnd_buf_append_str(out, writer->eol))
            {
                return false;
            }

            line_start = out->len;
            if (!mnd_buf_append_str(out, "#:"))
            {
                return false;
            }
        }

        if (!mnd_buf_append_byte(out, ' ')
            || (isolate && !mnd_buf_append_str(out, "\xE2\x81\xA8"))
            || !mnd_buf_append(out, ref->file.data, ref->file.len)
            || (isolate && !mnd_buf_append_str(out, "\xE2\x81\xA9"))
            || !mnd_buf_append(out, number, number_len))
        {
            return false;
        }
    }

    return mnd_buf_append_str(out, writer->eol);
}

/* fuzzy 以外のフラグを 1 つ以上持つか */
static bool mnd_po_has_non_fuzzy_flag(const mnd_po_entry_t *entry)
{
    size_t i;

    for (i = 0; i < entry->flags.count; i++)
    {
        if (strcmp(mnd_buf_cstr(&entry->flags.items[i]), "fuzzy") != 0)
        {
            return true;
        }
    }

    return false;
}

/*
 * フラグの行を書く。fuzzy は書く条件（5.7）を満たせば先頭に書き、ほかはエントリの順に書く。
 * fuzzy を書く条件は状態の判定（1.5）が FUZZY になることと同じにする（fuzzy フラグがあり、
 * msgstr が 1 個以上で最初の msgstr が空でない）。書くフラグがなければ行を書かない。
 * エントリの flags は変更しない
 */
static bool mnd_po_write_flags(mnd_po_writer_t *writer, const mnd_po_entry_t *entry)
{
    mnd_buf_t *out = writer->out;
    const char *separator = " ";
    bool write_fuzzy = mnd_po_entry_state(entry) == MND_PO_STATE_FUZZY;
    size_t i;

    if (!write_fuzzy && !mnd_po_has_non_fuzzy_flag(entry))
    {
        return true;
    }

    if (!mnd_buf_append_str(out, "#,"))
    {
        return false;
    }

    if (write_fuzzy)
    {
        if (!mnd_buf_append_str(out, " fuzzy"))
        {
            return false;
        }

        separator = ", ";
    }

    for (i = 0; i < entry->flags.count; i++)
    {
        const mnd_buf_t *flag = &entry->flags.items[i];

        if (strcmp(mnd_buf_cstr(flag), "fuzzy") == 0)
        {
            continue;
        }

        if (!mnd_buf_append_str(out, separator) || !mnd_buf_append(out, flag->data, flag->len))
        {
            return false;
        }

        separator = ", ";
    }

    return mnd_buf_append_str(out, writer->eol);
}

/* 文字列 1 つを Wrap で書く */
static bool mnd_po_write_string(mnd_po_writer_t *writer, const char *prefix, const char *keyword,
                                const mnd_buf_t *s, bool wrap)
{
    return mnd_po_wrap_string(prefix, keyword, mnd_buf_cstr(s), s->len, wrap, writer->eol,
                              writer->out);
}

/* 1 つのエントリを、設計の表の順に書く */
static bool mnd_po_write_entry(mnd_po_writer_t *writer, const mnd_po_entry_t *entry)
{
    const char *p = entry->obsolete ? "#~ " : "";
    const char *q = entry->obsolete ? "#~| " : "#| ";
    bool wrap = !writer->no_wrap && !mnd_po_flag_has(entry, "no-wrap");
    static const mnd_buf_t empty = MND_BUF_INIT;

    if (!mnd_po_write_comments(writer, "#", &entry->translator_comments)
        || !mnd_po_write_comments(writer, "#.", &entry->extracted_comments)
        || !mnd_po_write_refs(writer, &entry->refs) || !mnd_po_write_flags(writer, entry))
    {
        return false;
    }

    if ((entry->has_prev_msgctxt
         && !mnd_po_write_string(writer, q, "msgctxt", &entry->prev_msgctxt, wrap))
        || (entry->has_prev_msgid
            && !mnd_po_write_string(writer, q, "msgid", &entry->prev_msgid, wrap))
        || (entry->has_prev_msgid_plural
            && !mnd_po_write_string(writer, q, "msgid_plural", &entry->prev_msgid_plural, wrap)))
    {
        return false;
    }

    if ((entry->has_msgctxt && !mnd_po_write_string(writer, p, "msgctxt", &entry->msgctxt, wrap))
        || !mnd_po_write_string(writer, p, "msgid", &entry->msgid, wrap)
        || (entry->has_msgid_plural
            && !mnd_po_write_string(writer, p, "msgid_plural", &entry->msgid_plural, wrap)))
    {
        return false;
    }

    if (!entry->has_msgid_plural)
    {
        /* 単数形は 1 つ目だけを書く（0 個なら空文字列） */
        const mnd_buf_t *msgstr = (entry->msgstr.count > 0) ? &entry->msgstr.items[0] : &empty;

        return mnd_po_write_string(writer, p, "msgstr", msgstr, wrap);
    }

    if (entry->msgstr.count == 0)
    {
        return mnd_po_write_string(writer, p, "msgstr[0]", &empty, wrap)
            && mnd_po_write_string(writer, p, "msgstr[1]", &empty, wrap);
    }

    {
        size_t i;

        for (i = 0; i < entry->msgstr.count; i++)
        {
            /* "msgstr[" + 数字 + "]" + NUL */
            char keyword[48] = "msgstr[";
            size_t prefix_len = strlen(keyword);
            size_t digits = mnd_po_format_decimal(i, 0, keyword + prefix_len,
                                                  sizeof(keyword) - prefix_len - 1);

            if (digits == 0)
            {
                return false;
            }

            keyword[prefix_len + digits] = ']';
            keyword[prefix_len + digits + 1] = '\0';
            if (!mnd_po_write_string(writer, p, keyword, &entry->msgstr.items[i], wrap))
            {
                return false;
            }
        }
    }

    return true;
}

/*
 * 書き出さないエントリか（5.8）。msgstr がないか最初の msgstr が空の廃止エントリは、コメント・フラグ・
 * 前回の msgid を含めて 1 行も書かない（gettext 0.21 と同じ）。判定は状態（1.5）・fuzzy（5.7）と同じ条件
 */
static bool mnd_po_write_skips(const mnd_po_entry_t *entry)
{
    return entry->obsolete && !mnd_po_entry_first_msgstr_is_nonempty(entry);
}

bool mnd_po_write(const mnd_po_catalog_t *catalog, const mnd_po_write_options_t *options,
                  mnd_buf_t *out, mnd_diag_list_t *diags)
{
    mnd_po_writer_t writer;
    size_t start = out->len;
    size_t i;
    bool written = false;

    if (mnd_po_write_report_duplicates(catalog, diags))
    {
        return false;
    }

    writer.eol = ((options != NULL) && (options->eol == MND_EOL_CRLF)) ? "\r\n" : "\n";
    writer.no_wrap = (options != NULL) && options->no_wrap;
    writer.out = out;

    for (i = 0; i < catalog->count; i++)
    {
        if (mnd_po_write_skips(&catalog->entries[i]))
        {
            continue;
        }

        /* 空行の区切りは実際に書いたエントリの間にだけ置く（5.1） */
        if ((written && !mnd_buf_append_str(out, writer.eol))
            || !mnd_po_write_entry(&writer, &catalog->entries[i]))
        {
            out->len = start;
            if (out->data != NULL)
            {
                out->data[start] = '\0';
            }

            mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory");

            return false;
        }

        written = true;
    }

    return true;
}
