/* Model: ヘッダの項目の取得と設定、テンプレート用ヘッダ、日時の書式 */

#include "po/po.h"
#include "po/po_internal.h"

#include <string.h>

/* オフセットの絶対値の上限（時と分を 2 桁ずつで表せる範囲） */
#define MND_PO_DATE_MAX_OFFSET ((99L * 60L) + 59L)

/* ヘッダの msgstr の 1 つの項目の位置。オフセットは msgstr の先頭が基準 */
typedef struct mnd_po_header_field
{
    size_t colon;        /* 名前の直後の ':' の位置 */
    size_t value_start;  /* 値の先頭（':' の後ろの空白を除く） */
    size_t line_end;     /* 行の終わり（'\n' の位置、または msgstr の長さ） */
} mnd_po_header_field_t;

static unsigned char mnd_po_ascii_lower(unsigned char c)
{
    if ((c >= 'A') && (c <= 'Z'))
    {
        return (unsigned char)(c - 'A' + 'a');
    }

    return c;
}

/* data の len バイトが NUL 終端の name と、ASCII の大文字と小文字を区別せずに等しければ true */
static bool mnd_po_name_equals(const char *data, size_t len, const char *name)
{
    size_t i;

    for (i = 0; i < len; i++)
    {
        if (name[i] == '\0')
        {
            return false;
        }

        if (mnd_po_ascii_lower((unsigned char)data[i])
            != mnd_po_ascii_lower((unsigned char)name[i]))
        {
            return false;
        }
    }

    return (name[len] == '\0');
}

/*
 * msgstr を '\n' で行に分け、最初の ':' より前が name に一致する最初の行を探す。
 * ':' のない行は項目として扱わない
 */
static bool mnd_po_header_find(const mnd_buf_t *msgstr, const char *name,
                               mnd_po_header_field_t *field)
{
    const char *data = mnd_buf_cstr(msgstr);
    size_t len = msgstr->len;
    size_t line_start = 0;

    while (line_start < len)
    {
        const char *newline = memchr(data + line_start, '\n', len - line_start);
        size_t line_end = (newline != NULL) ? (size_t)(newline - data) : len;
        const char *colon = memchr(data + line_start, ':', line_end - line_start);

        if ((colon != NULL)
            && mnd_po_name_equals(data + line_start, (size_t)(colon - data) - line_start, name))
        {
            size_t value_start = (size_t)(colon - data) + 1;

            while ((value_start < line_end)
                   && ((data[value_start] == ' ') || (data[value_start] == '\t')))
            {
                value_start++;
            }

            field->colon = (size_t)(colon - data);
            field->value_start = value_start;
            field->line_end = line_end;

            return true;
        }

        line_start = line_end + 1;
    }

    return false;
}

bool mnd_po_header_get(const mnd_po_entry_t *header, const char *name, mnd_slice_t *value)
{
    mnd_po_header_field_t field;
    const mnd_buf_t *msgstr;

    if (header->msgstr.count == 0)
    {
        return false;
    }

    msgstr = &header->msgstr.items[0];
    if (!mnd_po_header_find(msgstr, name, &field))
    {
        return false;
    }

    value->data = mnd_buf_cstr(msgstr) + field.value_start;
    value->len = field.line_end - field.value_start;

    return true;
}

bool mnd_po_header_set(mnd_po_entry_t *header, const char *name, const char *value)
{
    mnd_po_header_field_t field;
    mnd_buf_t result = MND_BUF_INIT;
    mnd_buf_t *msgstr;
    const char *data;
    bool ok;

    if (header->msgstr.count == 0)
    {
        if (!mnd_po_strings_push(&header->msgstr, "", 0))
        {
            return false;
        }
    }

    msgstr = &header->msgstr.items[0];
    data = mnd_buf_cstr(msgstr);

    /*
     * 新しい内容を別のバッファに作ってから入れ替える。
     * メモリ不足のときは元の msgstr を変更しない
     */
    if (mnd_po_header_find(msgstr, name, &field))
    {
        ok = mnd_buf_append(&result, data, field.colon)
             && mnd_buf_append_str(&result, ": ")
             && mnd_buf_append_str(&result, value)
             && mnd_buf_append(&result, data + field.line_end, msgstr->len - field.line_end);
    }
    else
    {
        ok = mnd_buf_append(&result, data, msgstr->len);
        if (ok && (msgstr->len > 0) && (data[msgstr->len - 1] != '\n'))
        {
            ok = mnd_buf_append_byte(&result, '\n');
        }

        ok = ok
             && mnd_buf_append_str(&result, name)
             && mnd_buf_append_str(&result, ": ")
             && mnd_buf_append_str(&result, value)
             && mnd_buf_append_byte(&result, '\n');
    }

    if (!ok)
    {
        mnd_buf_free(&result);
        return false;
    }

    mnd_buf_free(msgstr);
    *msgstr = result;

    return true;
}

bool mnd_po_header_template(mnd_po_entry_t *out, const char *project_id_version,
                            const char *creation_date)
{
    mnd_buf_t msgstr = MND_BUF_INIT;
    bool ok;

    if (project_id_version == NULL)
    {
        project_id_version = "PACKAGE VERSION";
    }

    ok = mnd_buf_append_str(&msgstr, "Project-Id-Version: ")
         && mnd_buf_append_str(&msgstr, project_id_version)
         && mnd_buf_append_str(&msgstr, "\nReport-Msgid-Bugs-To: \nPOT-Creation-Date: ")
         && mnd_buf_append_str(&msgstr, creation_date)
         && mnd_buf_append_str(&msgstr,
                               "\n"
                               "PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\n"
                               "Last-Translator: FULL NAME <EMAIL@ADDRESS>\n"
                               "Language-Team: LANGUAGE <LL@li.org>\n"
                               "Language: \n"
                               "MIME-Version: 1.0\n"
                               "Content-Type: text/plain; charset=UTF-8\n"
                               "Content-Transfer-Encoding: 8bit\n");
    ok = ok
         && mnd_po_flag_add(out, "fuzzy")
         && mnd_po_strings_push(&out->msgstr, mnd_buf_cstr(&msgstr), msgstr.len);
    mnd_buf_free(&msgstr);

    return ok;
}

bool mnd_po_date_format(const struct tm *local_time, long utc_offset_minutes,
                        char out[MND_PO_DATE_SIZE])
{
    /* "YYYY-MM-DD HH:MM+HHMM" の各欄（値と桁数）と、その直後に書く区切り（最後は符号） */
    struct
    {
        size_t value;
        size_t digits;
        char after;
    } fields[7];
    long offset;
    size_t pos = 0;
    size_t i;

    /* 書式の桁数に収まらない値は扱わない（tm_year + 1900 の桁あふれも防ぐ） */
    if ((local_time->tm_year < -1900) || (local_time->tm_year > (9999 - 1900))
        || (local_time->tm_mon < 0) || (local_time->tm_mon > 11)
        || (local_time->tm_mday < 1) || (local_time->tm_mday > 31)
        || (local_time->tm_hour < 0) || (local_time->tm_hour > 23)
        || (local_time->tm_min < 0) || (local_time->tm_min > 59)
        || (utc_offset_minutes > MND_PO_DATE_MAX_OFFSET)
        || (utc_offset_minutes < -MND_PO_DATE_MAX_OFFSET))
    {
        return false;
    }

    offset = (utc_offset_minutes < 0) ? -utc_offset_minutes : utc_offset_minutes;
    fields[0].value = (size_t)(local_time->tm_year + 1900);
    fields[0].digits = 4;
    fields[0].after = '-';
    fields[1].value = (size_t)(local_time->tm_mon + 1);
    fields[1].digits = 2;
    fields[1].after = '-';
    fields[2].value = (size_t)local_time->tm_mday;
    fields[2].digits = 2;
    fields[2].after = ' ';
    fields[3].value = (size_t)local_time->tm_hour;
    fields[3].digits = 2;
    fields[3].after = ':';
    fields[4].value = (size_t)local_time->tm_min;
    fields[4].digits = 2;
    fields[4].after = (utc_offset_minutes < 0) ? '-' : '+';
    fields[5].value = (size_t)(offset / 60);
    fields[5].digits = 2;
    fields[5].after = '\0';
    fields[6].value = (size_t)(offset % 60);
    fields[6].digits = 2;
    fields[6].after = '\0';

    for (i = 0; i < (sizeof(fields) / sizeof(fields[0])); i++)
    {
        size_t digits = mnd_po_format_decimal(fields[i].value, fields[i].digits, out + pos,
                                              MND_PO_DATE_SIZE - pos);

        if (digits == 0)
        {
            return false;
        }

        pos += digits;
        if (fields[i].after != '\0')
        {
            if (pos + 1 >= MND_PO_DATE_SIZE)
            {
                return false;
            }

            out[pos] = fields[i].after;
            pos++;
            out[pos] = '\0';
        }
    }

    return true;
}
