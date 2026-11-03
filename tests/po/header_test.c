/* Model: ヘッダの項目の取得と設定、テンプレート用ヘッダ、日時の書式 */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "po/po.h"

#include <string.h>
#include <time.h>

/* テスト用: msgstr を 1 個持つヘッダのエントリを作る */
static bool make_header(mnd_po_entry_t *entry, const char *msgstr)
{
    mnd_po_entry_init(entry);

    return mnd_po_strings_push(&entry->msgstr, msgstr, strlen(msgstr));
}

static const char *header_msgstr(const mnd_po_entry_t *entry)
{
    return mnd_buf_cstr(&entry->msgstr.items[0]);
}

static struct tm make_tm(int year, int mon, int mday, int hour, int min)
{
    struct tm t;

    memset(&t, 0, sizeof(t));
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = mday;
    t.tm_hour = hour;
    t.tm_min = min;

    return t;
}

NAOS_TEST(header_date_size_holds_po_date)
{
    /* "YYYY-MM-DD HH:MM+ZZZZ" と終端の NUL が収まる */
    NAOS_CHECK_UINT_GE(MND_PO_DATE_SIZE, sizeof("YYYY-MM-DD HH:MM+ZZZZ"));
}

NAOS_TEST(header_get_matches_name_case_insensitively)
{
    mnd_po_entry_t entry;
    mnd_slice_t value;
    const char *msgstr = "Project-Id-Version: demo 1.0\n"
                         "Content-Type: text/plain; charset=UTF-8\n"
                         "Language: ja\n";

    NAOS_REQUIRE_TRUE(make_header(&entry, msgstr));

    NAOS_REQUIRE_TRUE(mnd_po_header_get(&entry, "language", &value));
    NAOS_CHECK_MEM_EQ(value.data, value.len, "ja", (size_t)2);
    /* 値はヘッダの msgstr の内部を指す */
    NAOS_CHECK_PTR_EQ(value.data, header_msgstr(&entry) + strlen(msgstr) - 3);

    NAOS_REQUIRE_TRUE(mnd_po_header_get(&entry, "CONTENT-TYPE", &value));
    NAOS_CHECK_MEM_EQ(value.data, value.len, "text/plain; charset=UTF-8",
                      strlen("text/plain; charset=UTF-8"));

    NAOS_REQUIRE_TRUE(mnd_po_header_get(&entry, "Project-Id-Version", &value));
    NAOS_CHECK_MEM_EQ(value.data, value.len, "demo 1.0", (size_t)8);

    mnd_po_entry_free(&entry);
}

NAOS_TEST(header_get_returns_empty_value_and_last_line_without_newline)
{
    mnd_po_entry_t entry;
    mnd_slice_t value;

    NAOS_REQUIRE_TRUE(make_header(&entry, "Report-Msgid-Bugs-To: \nLanguage:\tfr"));

    NAOS_REQUIRE_TRUE(mnd_po_header_get(&entry, "Report-Msgid-Bugs-To", &value));
    NAOS_CHECK_UINT_EQ(0, value.len);

    NAOS_REQUIRE_TRUE(mnd_po_header_get(&entry, "Language", &value));
    NAOS_CHECK_MEM_EQ(value.data, value.len, "fr", (size_t)2);

    mnd_po_entry_free(&entry);
}

NAOS_TEST(header_get_reports_missing_field)
{
    mnd_po_entry_t entry;
    mnd_po_entry_t empty;
    mnd_slice_t value;

    /* ':' のない行と、名前の一部だけが一致する行は項目として一致しない */
    NAOS_REQUIRE_TRUE(make_header(&entry, "Language\nLanguage-Team: x\nLang: y\n"));
    NAOS_CHECK_FALSE(mnd_po_header_get(&entry, "Language", &value));
    NAOS_CHECK_FALSE(mnd_po_header_get(&entry, "Plural-Forms", &value));
    mnd_po_entry_free(&entry);

    /* msgstr を持たないエントリ */
    mnd_po_entry_init(&empty);
    NAOS_CHECK_FALSE(mnd_po_header_get(&empty, "Language", &value));
    mnd_po_entry_free(&empty);
}

NAOS_TEST(header_set_replaces_value_keeping_order_and_spelling)
{
    mnd_po_entry_t entry;
    mnd_slice_t value;

    NAOS_REQUIRE_TRUE(make_header(&entry,
                                  "Project-Id-Version: demo\n"
                                  "LANGUAGE: \n"
                                  "MIME-Version: 1.0\n"));

    NAOS_REQUIRE_TRUE(mnd_po_header_set(&entry, "language", "ja"));
    NAOS_CHECK_UINT_EQ(1, entry.msgstr.count);
    NAOS_CHECK_STR_EQ(header_msgstr(&entry),
                      "Project-Id-Version: demo\n"
                      "LANGUAGE: ja\n"
                      "MIME-Version: 1.0\n");

    /* 値を長い値に置き換えても、ほかの項目は変わらない */
    NAOS_REQUIRE_TRUE(mnd_po_header_set(&entry, "Project-Id-Version", "demo 2.0 beta"));
    NAOS_CHECK_STR_EQ(header_msgstr(&entry),
                      "Project-Id-Version: demo 2.0 beta\n"
                      "LANGUAGE: ja\n"
                      "MIME-Version: 1.0\n");

    NAOS_REQUIRE_TRUE(mnd_po_header_get(&entry, "Language", &value));
    NAOS_CHECK_MEM_EQ(value.data, value.len, "ja", (size_t)2);

    mnd_po_entry_free(&entry);
}

NAOS_TEST(header_set_appends_missing_field_at_end)
{
    mnd_po_entry_t entry;

    NAOS_REQUIRE_TRUE(make_header(&entry, "MIME-Version: 1.0\n"));
    NAOS_REQUIRE_TRUE(mnd_po_header_set(&entry, "Language", "ja"));
    NAOS_CHECK_STR_EQ(header_msgstr(&entry), "MIME-Version: 1.0\nLanguage: ja\n");
    mnd_po_entry_free(&entry);
}

NAOS_TEST(header_set_appends_newline_before_field)
{
    mnd_po_entry_t entry;
    mnd_po_entry_t empty;

    /* msgstr が改行で終わっていなければ、先に改行を加える */
    NAOS_REQUIRE_TRUE(make_header(&entry, "MIME-Version: 1.0"));
    NAOS_REQUIRE_TRUE(mnd_po_header_set(&entry, "Language", "ja"));
    NAOS_CHECK_STR_EQ(header_msgstr(&entry), "MIME-Version: 1.0\nLanguage: ja\n");
    mnd_po_entry_free(&entry);

    /* 空の msgstr と、msgstr のないエントリには改行を加えない */
    NAOS_REQUIRE_TRUE(make_header(&entry, ""));
    NAOS_REQUIRE_TRUE(mnd_po_header_set(&entry, "Language", "ja"));
    NAOS_CHECK_STR_EQ(header_msgstr(&entry), "Language: ja\n");
    mnd_po_entry_free(&entry);

    mnd_po_entry_init(&empty);
    NAOS_REQUIRE_TRUE(mnd_po_header_set(&empty, "Language", "ja"));
    NAOS_REQUIRE_UINT_EQ(1, empty.msgstr.count);
    NAOS_CHECK_STR_EQ(header_msgstr(&empty), "Language: ja\n");
    mnd_po_entry_free(&empty);
}

NAOS_TEST(header_template_matches_xgettext_header)
{
    static const char expected[] =
        "Project-Id-Version: monde 0.1\n"
        "Report-Msgid-Bugs-To: \n"
        "POT-Creation-Date: 2026-11-03 09:05+0900\n"
        "PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\n"
        "Last-Translator: FULL NAME <EMAIL@ADDRESS>\n"
        "Language-Team: LANGUAGE <LL@li.org>\n"
        "Language: \n"
        "MIME-Version: 1.0\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8bit\n";
    mnd_po_entry_t entry;

    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_header_template(&entry, "monde 0.1", "2026-11-03 09:05+0900"));

    NAOS_REQUIRE_UINT_EQ(1, entry.msgstr.count);
    NAOS_CHECK_MEM_EQ(entry.msgstr.items[0].data, entry.msgstr.items[0].len,
                      expected, sizeof(expected) - 1);
    NAOS_CHECK_UINT_EQ(0, entry.msgid.len);
    NAOS_CHECK_FALSE(entry.has_msgctxt);
    NAOS_CHECK_FALSE(entry.has_msgid_plural);
    NAOS_CHECK_FALSE(entry.obsolete);
    NAOS_CHECK_UINT_EQ(0, entry.translator_comments.count);
    NAOS_CHECK_UINT_EQ(0, entry.extracted_comments.count);
    NAOS_CHECK_UINT_EQ(0, entry.refs.count);
    NAOS_REQUIRE_UINT_EQ(1, entry.flags.count);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry.flags.items[0]), "fuzzy");
    NAOS_CHECK_INT_EQ(MND_PO_STATE_FUZZY, mnd_po_entry_state(&entry));

    mnd_po_entry_free(&entry);
}

NAOS_TEST(header_template_uses_default_project_id_version)
{
    static const char expected[] =
        "Project-Id-Version: PACKAGE VERSION\n"
        "Report-Msgid-Bugs-To: \n"
        "POT-Creation-Date: 2000-01-02 03:04-0130\n"
        "PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\n"
        "Last-Translator: FULL NAME <EMAIL@ADDRESS>\n"
        "Language-Team: LANGUAGE <LL@li.org>\n"
        "Language: \n"
        "MIME-Version: 1.0\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8bit\n";
    mnd_po_entry_t entry;

    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_header_template(&entry, NULL, "2000-01-02 03:04-0130"));
    NAOS_REQUIRE_UINT_EQ(1, entry.msgstr.count);
    NAOS_CHECK_MEM_EQ(entry.msgstr.items[0].data, entry.msgstr.items[0].len,
                      expected, sizeof(expected) - 1);
    mnd_po_entry_free(&entry);
}

NAOS_TEST(header_date_format_positive_offset)
{
    struct tm t = make_tm(2026, 11, 3, 9, 5);
    char out[MND_PO_DATE_SIZE];

    NAOS_REQUIRE_TRUE(mnd_po_date_format(&t, 9L * 60L, out));
    NAOS_CHECK_STR_EQ(out, "2026-11-03 09:05+0900");

    NAOS_REQUIRE_TRUE(mnd_po_date_format(&t, (5L * 60L) + 45L, out));
    NAOS_CHECK_STR_EQ(out, "2026-11-03 09:05+0545");

    NAOS_REQUIRE_TRUE(mnd_po_date_format(&t, 0L, out));
    NAOS_CHECK_STR_EQ(out, "2026-11-03 09:05+0000");
}

NAOS_TEST(header_date_format_negative_offset)
{
    struct tm t = make_tm(1999, 1, 31, 23, 59);
    char out[MND_PO_DATE_SIZE];

    NAOS_REQUIRE_TRUE(mnd_po_date_format(&t, -(3L * 60L) - 30L, out));
    NAOS_CHECK_STR_EQ(out, "1999-01-31 23:59-0330");

    NAOS_REQUIRE_TRUE(mnd_po_date_format(&t, -8L * 60L, out));
    NAOS_CHECK_STR_EQ(out, "1999-01-31 23:59-0800");
}

NAOS_TEST(header_date_format_rejects_out_of_range_values)
{
    struct tm t = make_tm(2026, 11, 3, 9, 5);
    struct tm bad_year = make_tm(10000, 1, 1, 0, 0);
    struct tm bad_hour = make_tm(2026, 1, 1, 24, 0);
    char out[MND_PO_DATE_SIZE];

    NAOS_CHECK_FALSE(mnd_po_date_format(&t, 100L * 60L, out));
    NAOS_CHECK_FALSE(mnd_po_date_format(&t, -100L * 60L, out));
    NAOS_CHECK_FALSE(mnd_po_date_format(&bad_year, 0L, out));
    NAOS_CHECK_FALSE(mnd_po_date_format(&bad_hour, 0L, out));
}

NAOS_SUITE(header,
    NAOS_ADD_TEST(header_date_size_holds_po_date),
    NAOS_ADD_TEST(header_get_matches_name_case_insensitively),
    NAOS_ADD_TEST(header_get_returns_empty_value_and_last_line_without_newline),
    NAOS_ADD_TEST(header_get_reports_missing_field),
    NAOS_ADD_TEST(header_set_replaces_value_keeping_order_and_spelling),
    NAOS_ADD_TEST(header_set_appends_missing_field_at_end),
    NAOS_ADD_TEST(header_set_appends_newline_before_field),
    NAOS_ADD_TEST(header_template_matches_xgettext_header),
    NAOS_ADD_TEST(header_template_uses_default_project_id_version),
    NAOS_ADD_TEST(header_date_format_positive_offset),
    NAOS_ADD_TEST(header_date_format_negative_offset),
    NAOS_ADD_TEST(header_date_format_rejects_out_of_range_values)
)
