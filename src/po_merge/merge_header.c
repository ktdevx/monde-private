/* Header: ヘッダの msgstr のマージ、ヘッダのエントリの組み立て、nplurals の読み取り */

#include "po_merge/merge_internal.h"

#include <string.h>

/* 既知の項目の数 */
#define MERGE_HEADER_FIELD_COUNT 10

/* 表の添字 */
#define MERGE_HEADER_REPORT_MSGID_BUGS_TO 1
#define MERGE_HEADER_POT_CREATION_DATE 2
#define MERGE_HEADER_LANGUAGE_TEAM 5
#define MERGE_HEADER_LANGUAGE 6

/* 既知の項目の名前（表の順と綴り） */
static const char *const merge_header_names[MERGE_HEADER_FIELD_COUNT] =
{
    "Project-Id-Version:",
    "Report-Msgid-Bugs-To:",
    "POT-Creation-Date:",
    "PO-Revision-Date:",
    "Last-Translator:",
    "Language-Team:",
    "Language:",
    "MIME-Version:",
    "Content-Type:",
    "Content-Transfer-Encoding:"
};

/* Language を補うときの値 */
static const char merge_header_empty_language[] = " \n";

/*
 * 項目の値。名前の直後から行の残り。add_newline が真なら、data の後に \n を補う
 * （data は \n で終わらない）
 */
typedef struct merge_header_value
{
    bool present;
    const char *data;
    size_t len;
    bool add_newline;
} merge_header_value_t;

/* ASCII の大文字を小文字にする */
static unsigned char merge_header_ascii_lower(unsigned char c)
{
    if ((c >= 'A') && (c <= 'Z'))
    {
        return (unsigned char)(c - 'A' + 'a');
    }

    return c;
}

/* line（len バイト）の先頭が name と ASCII の大文字・小文字を区別せずに一致すれば真 */
static bool merge_header_starts_with_name(const char *line, size_t len, const char *name)
{
    size_t name_len = strlen(name);
    size_t i;

    if (len < name_len)
    {
        return false;
    }

    for (i = 0; i < name_len; i++)
    {
        if (merge_header_ascii_lower((unsigned char)line[i])
            != merge_header_ascii_lower((unsigned char)name[i]))
        {
            return false;
        }
    }

    return true;
}

/* line（len バイト）に一致する既知の項目の添字を返す。なければ MERGE_HEADER_FIELD_COUNT */
static size_t merge_header_find_field(const char *line, size_t len)
{
    size_t i;

    for (i = 0; i < MERGE_HEADER_FIELD_COUNT; i++)
    {
        if (merge_header_starts_with_name(line, len, merge_header_names[i]))
        {
            return i;
        }
    }

    return MERGE_HEADER_FIELD_COUNT;
}

/* data[pos..len) の次の行の長さ（\n があれば含む）を返す */
static size_t merge_header_line_length(const char *data, size_t len, size_t pos)
{
    const char *newline = memchr(data + pos, '\n', len - pos);

    if (newline == NULL)
    {
        return len - pos;
    }

    return (size_t)(newline - (data + pos)) + 1;
}

/* data（len バイト）から始まる行の残りを value にする。\n で終わらなければ \n を補う */
static void merge_header_set_value(merge_header_value_t *value, const char *data, size_t len)
{
    value->present = true;
    value->data = data;
    value->len = len;
    value->add_newline = (len == 0) || (data[len - 1] != '\n');
}

/*
 * 手順 4: template_msgstr の中で name を（大文字・小文字を区別して）最初に見つかった位置から探し、
 * 名前の直後から行末の \n まで（なければ文字列の終わりまで）を value にする
 */
static void merge_header_take_template_value(merge_header_value_t *value,
                                             const mnd_buf_t *template_msgstr, const char *name)
{
    const char *data = mnd_buf_cstr(template_msgstr);
    const char *found = strstr(data, name);
    const char *start;
    size_t rest;
    const char *newline;

    if (found == NULL)
    {
        return;
    }

    start = found + strlen(name);
    rest = template_msgstr->len - (size_t)(start - data);
    newline = memchr(start, '\n', rest);
    if (newline != NULL)
    {
        rest = (size_t)(newline - start) + 1;
    }

    merge_header_set_value(value, start, rest);
}

/* 値の内容（必要なら \n を補って）を out に加える */
static bool merge_header_append_value(mnd_buf_t *out, const merge_header_value_t *value)
{
    if (!mnd_buf_append(out, value->data, value->len))
    {
        return false;
    }

    if (value->add_newline)
    {
        return mnd_buf_append_byte(out, (unsigned char)'\n');
    }

    return true;
}

/* 「ヘッダの規則」の手順 1〜5 で msgstr を out（初期化済み）に作る */
static bool merge_header_build_msgstr(mnd_buf_t *out, const mnd_po_entry_t *old_header,
                                      const mnd_po_entry_t *template_header)
{
    merge_header_value_t values[MERGE_HEADER_FIELD_COUNT];
    const char *data = "";
    size_t len = 0;
    size_t pos;
    size_t line_len;
    size_t field;
    size_t name_len;
    size_t i;

    memset(values, 0, sizeof(values));
    if (old_header->msgstr.count > 0)
    {
        data = mnd_buf_cstr(&old_header->msgstr.items[0]);
        len = old_header->msgstr.items[0].len;
    }

    /* 手順 1・2: 行に分け、既知の項目の値を記録する（後の行が優先） */
    for (pos = 0; pos < len; pos += line_len)
    {
        line_len = merge_header_line_length(data, len, pos);
        field = merge_header_find_field(data + pos, line_len);
        if (field < MERGE_HEADER_FIELD_COUNT)
        {
            name_len = strlen(merge_header_names[field]);
            merge_header_set_value(&values[field], data + pos + name_len, line_len - name_len);
        }
    }

    /* 手順 3: Language-Team だけがあれば空の Language を補う */
    if ((!values[MERGE_HEADER_LANGUAGE].present) && values[MERGE_HEADER_LANGUAGE_TEAM].present)
    {
        merge_header_set_value(&values[MERGE_HEADER_LANGUAGE], merge_header_empty_language,
                               sizeof(merge_header_empty_language) - 1);
    }

    /* 手順 4: テンプレートの値で置き換える */
    if ((template_header != NULL) && (template_header->msgstr.count > 0))
    {
        merge_header_take_template_value(&values[MERGE_HEADER_REPORT_MSGID_BUGS_TO],
                                         &template_header->msgstr.items[0],
                                         merge_header_names[MERGE_HEADER_REPORT_MSGID_BUGS_TO]);
        merge_header_take_template_value(&values[MERGE_HEADER_POT_CREATION_DATE],
                                         &template_header->msgstr.items[0],
                                         merge_header_names[MERGE_HEADER_POT_CREATION_DATE]);
    }

    /* 手順 5: 表の順と綴りで既知の項目を並べ、その後にほかの行を元の順で並べる */
    for (i = 0; i < MERGE_HEADER_FIELD_COUNT; i++)
    {
        if (!values[i].present)
        {
            continue;
        }

        if ((!mnd_buf_append_str(out, merge_header_names[i]))
            || (!merge_header_append_value(out, &values[i])))
        {
            return false;
        }
    }

    for (pos = 0; pos < len; pos += line_len)
    {
        line_len = merge_header_line_length(data, len, pos);
        if (merge_header_find_field(data + pos, line_len) < MERGE_HEADER_FIELD_COUNT)
        {
            continue;
        }

        if (!mnd_buf_append(out, data + pos, line_len))
        {
            return false;
        }

        if ((data[pos + line_len - 1] != '\n') && (!mnd_buf_append_byte(out, (unsigned char)'\n')))
        {
            return false;
        }
    }

    return true;
}

/* src の文字列をすべて dst の末尾に複製する */
static bool merge_header_copy_strings(mnd_po_strings_t *dst, const mnd_po_strings_t *src)
{
    size_t i;

    for (i = 0; i < src->count; i++)
    {
        if (!mnd_po_strings_push(dst, mnd_buf_cstr(&src->items[i]), src->items[i].len))
        {
            return false;
        }
    }

    return true;
}

/* 翻訳者コメント・抽出コメント・参照・フラグ・msgid・msgstr を dst に作る */
static bool merge_header_build(mnd_po_entry_t *dst, const mnd_po_entry_t *old_header,
                               const mnd_po_entry_t *template_header)
{
    mnd_buf_t msgstr = MND_BUF_INIT;
    bool ok;
    size_t i;

    /* msgctxt なし、空の msgid（既存のヘッダと同じ） */
    if (!mnd_buf_append(&dst->msgid, mnd_buf_cstr(&old_header->msgid), old_header->msgid.len))
    {
        return false;
    }

    if (!merge_header_copy_strings(&dst->translator_comments, &old_header->translator_comments))
    {
        return false;
    }

    if (mnd_po_flag_has(old_header, "fuzzy"))
    {
        if (!mnd_po_strings_push(&dst->flags, "fuzzy", strlen("fuzzy")))
        {
            return false;
        }
    }

    if (template_header != NULL)
    {
        if (!merge_header_copy_strings(&dst->extracted_comments,
                                       &template_header->extracted_comments))
        {
            return false;
        }

        for (i = 0; i < template_header->refs.count; i++)
        {
            if (!mnd_po_refs_push(&dst->refs, mnd_buf_cstr(&template_header->refs.items[i].file),
                                  template_header->refs.items[i].line))
            {
                return false;
            }
        }

        for (i = 0; i < template_header->flags.count; i++)
        {
            if (strcmp(mnd_buf_cstr(&template_header->flags.items[i]), "fuzzy") == 0)
            {
                continue;
            }

            if (!mnd_po_strings_push(&dst->flags, mnd_buf_cstr(&template_header->flags.items[i]),
                                     template_header->flags.items[i].len))
            {
                return false;
            }
        }
    }

    ok = merge_header_build_msgstr(&msgstr, old_header, template_header)
         && mnd_po_strings_push(&dst->msgstr, mnd_buf_cstr(&msgstr), msgstr.len);
    mnd_buf_free(&msgstr);

    return ok;
}

bool mnd_merge_header(mnd_po_entry_t *dst, const mnd_po_entry_t *old_header,
                      const mnd_po_entry_t *template_header)
{
    if (!merge_header_build(dst, old_header, template_header))
    {
        mnd_po_entry_free(dst);
        return false;
    }

    return true;
}

/* 空白またはタブなら真 */
static bool merge_header_is_blank(char c)
{
    return (c == ' ') || (c == '\t');
}

size_t mnd_merge_nplurals(const mnd_po_entry_t *header)
{
    static const char keyword[] = "nplurals";
    const size_t keyword_len = sizeof(keyword) - 1;
    mnd_slice_t value;
    size_t pos;
    size_t result = 0;
    bool has_digit = false;

    if ((header == NULL) || (!mnd_po_header_get(header, "Plural-Forms", &value)))
    {
        return MND_MERGE_NPLURALS_DEFAULT;
    }

    /* 値の中で最初の nplurals を探す */
    for (pos = 0; (pos + keyword_len) <= value.len; pos++)
    {
        if (memcmp(value.data + pos, keyword, keyword_len) == 0)
        {
            break;
        }
    }

    if ((pos + keyword_len) > value.len)
    {
        return MND_MERGE_NPLURALS_DEFAULT;
    }

    pos += keyword_len;
    while ((pos < value.len) && merge_header_is_blank(value.data[pos]))
    {
        pos++;
    }

    if ((pos >= value.len) || (value.data[pos] != '='))
    {
        return MND_MERGE_NPLURALS_DEFAULT;
    }

    pos++;
    while ((pos < value.len) && merge_header_is_blank(value.data[pos]))
    {
        pos++;
    }

    /* 上限を超えたら上限 + 1 に留めて、桁あふれを防ぐ */
    while ((pos < value.len) && (value.data[pos] >= '0') && (value.data[pos] <= '9'))
    {
        has_digit = true;
        if (result <= MND_MERGE_NPLURALS_MAX)
        {
            result = (result * 10) + (size_t)(value.data[pos] - '0');
        }

        pos++;
    }

    if ((!has_digit) || (result < 1) || (result > MND_MERGE_NPLURALS_MAX))
    {
        return MND_MERGE_NPLURALS_DEFAULT;
    }

    return result;
}
