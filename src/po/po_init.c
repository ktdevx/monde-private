/* Init: テンプレートからの初期化 */

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/source.h"
#include "po/po.h"
#include "po/po_init.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 診断に表示する値の最大バイト数 */
#define MND_PO_INIT_SHOW_MAX 40

/* 空白（U+0020）と制御文字（0x00〜0x1F、0x7F）。0x00 は NUL 終端なので現れない */
static bool mnd_po_init_is_space_or_control(unsigned char c)
{
    return (c <= 0x20) || (c == 0x7F);
}

/* 言語コードが空でなく、空白と制御文字を含まなければ true（9.5） */
static bool mnd_po_init_language_valid(const char *language)
{
    size_t i;

    if (language[0] == '\0')
    {
        return false;
    }

    for (i = 0; language[i] != '\0'; i++)
    {
        if (mnd_po_init_is_space_or_control((unsigned char)language[i]))
        {
            return false;
        }
    }

    return true;
}

/*
 * s から始まる UTF-8 の 1 文字のバイト数を返す。正しい並びでなければ 0。
 * s は NUL 終端なので、継続バイトの検査は終端を越えない
 */
static size_t mnd_po_init_char_len(const unsigned char *s)
{
    size_t len;
    size_t i;

    if (s[0] < 0x80)
    {
        return 1;
    }

    if ((s[0] >= 0xC2) && (s[0] <= 0xDF))
    {
        len = 2;
    }
    else if ((s[0] >= 0xE0) && (s[0] <= 0xEF))
    {
        len = 3;
    }
    else if ((s[0] >= 0xF0) && (s[0] <= 0xF4))
    {
        len = 4;
    }
    else
    {
        return 0;
    }

    for (i = 1; i < len; i++)
    {
        if ((s[i] & 0xC0) != 0x80)
        {
            return 0;
        }
    }

    return len;
}

/*
 * 表示に使う値を out（NUL 終端）に作る。文字の途中で切らずに MND_PO_INIT_SHOW_MAX バイトまでとし、
 * 制御文字（C0、DEL、C1）を '?' にする。言語コードは利用者の入力なので、UTF-8 として不正なバイトも '?' にする
 */
static void mnd_po_init_show(const char *s, char out[MND_PO_INIT_SHOW_MAX + 1])
{
    const unsigned char *p = (const unsigned char *)s;
    size_t i = 0;
    size_t n = 0;

    while (p[i] != '\0')
    {
        size_t char_len = mnd_po_init_char_len(p + i);
        bool replace = (char_len == 0) || (p[i] < 0x20) || (p[i] == 0x7F)
                       || ((p[i] == 0xC2) && (p[i + 1] < 0xA0));
        size_t out_len = replace ? 1 : char_len;

        if (char_len == 0)
        {
            char_len = 1;
        }

        if (n + out_len > MND_PO_INIT_SHOW_MAX)
        {
            break;
        }

        if (replace)
        {
            out[n] = '?';
        }
        else
        {
            memcpy(out + n, s + i, char_len);
        }

        n += out_len;
        i += char_len;
    }

    out[n] = '\0';
}

/* data[0..len) が NUL 終端の name と、ASCII の大文字と小文字を区別せずに等しければ true */
static bool mnd_po_init_equals_nocase(const char *data, size_t len, const char *name)
{
    size_t i;

    for (i = 0; i < len; i++)
    {
        char x = data[i];
        char y = name[i];

        if (y == '\0')
        {
            return false;
        }

        if ((x >= 'a') && (x <= 'z'))
        {
            x = (char)(x - ('a' - 'A'));
        }

        if ((y >= 'a') && (y <= 'z'))
        {
            y = (char)(y - ('a' - 'A'));
        }

        if (x != y)
        {
            return false;
        }
    }

    return (name[len] == '\0');
}

static bool mnd_po_init_is_blank(char c)
{
    return (c == ' ') || (c == '\t');
}

/*
 * Content-Type の charset= の値（';' か行末まで、前後の空白を除く）が CHARSET（大文字と小文字を区別しない）なら、
 * その値だけを UTF-8 に置き換える（9.2）。メモリ不足なら false
 */
static bool mnd_po_init_fix_charset(mnd_po_entry_t *header)
{
    static const char key[] = "charset=";
    const size_t key_len = sizeof(key) - 1;
    mnd_buf_t value_buf = MND_BUF_INIT;
    mnd_slice_t value;
    size_t i;
    size_t start;
    size_t end;
    bool ok;

    if (!mnd_po_header_get(header, "Content-Type", &value))
    {
        return true;
    }

    for (i = 0; (i + key_len) <= value.len; i++)
    {
        if (mnd_po_init_equals_nocase(value.data + i, key_len, key))
        {
            break;
        }
    }

    if ((i + key_len) > value.len)
    {
        return true;
    }

    start = i + key_len;
    end = start;
    while ((end < value.len) && (value.data[end] != ';'))
    {
        end++;
    }

    while ((start < end) && mnd_po_init_is_blank(value.data[start]))
    {
        start++;
    }

    while ((end > start) && mnd_po_init_is_blank(value.data[end - 1]))
    {
        end--;
    }

    if (!mnd_po_init_equals_nocase(value.data + start, end - start, "CHARSET"))
    {
        return true;
    }

    /* value はヘッダの内部を指すので、新しい値を別のバッファに作ってから設定する */
    ok = mnd_buf_append(&value_buf, value.data, start)
         && mnd_buf_append_str(&value_buf, "UTF-8")
         && mnd_buf_append(&value_buf, value.data + end, value.len - end)
         && mnd_po_header_set(header, "Content-Type", mnd_buf_cstr(&value_buf));
    mnd_buf_free(&value_buf);

    return ok;
}

/* 既存のヘッダを翻訳ファイル用に変える（9.1〜9.3）。メモリ不足なら false */
static bool mnd_po_init_update_header(mnd_po_entry_t *header, const char *language,
                                      const char *revision_date)
{
    if (!mnd_po_header_set(header, "Language", language)
        || !mnd_po_header_set(header, "PO-Revision-Date", revision_date)
        || !mnd_po_init_fix_charset(header))
    {
        return false;
    }

    mnd_po_flag_remove(header, "fuzzy");

    return true;
}

/* 5 項目を持つ fuzzy でないヘッダを先頭に挿入する（9.4）。メモリ不足なら catalog を変えずに false */
static bool mnd_po_init_insert_header(mnd_po_catalog_t *catalog, const char *language,
                                      const char *revision_date)
{
    mnd_buf_t msgstr = MND_BUF_INIT;
    mnd_po_entry_t *header;
    bool ok;

    ok = mnd_buf_append_str(&msgstr, "PO-Revision-Date: ")
         && mnd_buf_append_str(&msgstr, revision_date)
         && mnd_buf_append_str(&msgstr, "\nLanguage: ")
         && mnd_buf_append_str(&msgstr, language)
         && mnd_buf_append_str(&msgstr,
                               "\n"
                               "MIME-Version: 1.0\n"
                               "Content-Type: text/plain; charset=UTF-8\n"
                               "Content-Transfer-Encoding: 8bit\n");
    if (!ok)
    {
        mnd_buf_free(&msgstr);
        return false;
    }

    header = mnd_po_catalog_insert(catalog, 0);
    if (header == NULL)
    {
        mnd_buf_free(&msgstr);
        return false;
    }

    ok = mnd_po_strings_push(&header->msgstr, mnd_buf_cstr(&msgstr), msgstr.len);
    mnd_buf_free(&msgstr);
    if (!ok)
    {
        /* 挿入したエントリを取り除き、catalog を呼び出し前の内容に戻す */
        mnd_po_entry_free(&catalog->entries[0]);
        memmove(&catalog->entries[0], &catalog->entries[1],
                (catalog->count - 1) * sizeof(catalog->entries[0]));
        catalog->count--;

        return false;
    }

    return true;
}

bool mnd_po_init(mnd_po_catalog_t *catalog, const char *language, const char *revision_date,
                 mnd_diag_list_t *diags)
{
    size_t index;
    bool ok;

    if (!mnd_po_init_language_valid(language))
    {
        char shown[MND_PO_INIT_SHOW_MAX + 1];

        mnd_po_init_show(language, shown);
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "invalid language code '%s'", shown);

        return false;
    }

    if (mnd_po_catalog_header(catalog, &index))
    {
        ok = mnd_po_init_update_header(&catalog->entries[index], language, revision_date);
    }
    else
    {
        ok = mnd_po_init_insert_header(catalog, language, revision_date);
    }

    if (!ok)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory");
        return false;
    }

    return true;
}
