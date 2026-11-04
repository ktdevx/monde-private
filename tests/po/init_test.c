/* Init: テンプレートからの初期化（9.1〜9.5） */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"
#include "po/po_init.h"
#include "po/po_read.h"
#include "po/po_write.h"

#include <string.h>

#define INIT_TEST_DATE "2026-11-04 12:34+0900"

/* xgettext が作るものと同じ形のテンプレート */
static const char init_template[] =
    "# SOME DESCRIPTIVE TITLE.\n"
    "# Copyright (C) YEAR THE PACKAGE'S COPYRIGHT HOLDER\n"
    "#\n"
    "#, fuzzy\n"
    "msgid \"\"\n"
    "msgstr \"\"\n"
    "\"Project-Id-Version: demo 1.0\\n\"\n"
    "\"Report-Msgid-Bugs-To: \\n\"\n"
    "\"POT-Creation-Date: 2026-01-02 03:04+0900\\n\"\n"
    "\"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n\"\n"
    "\"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n\"\n"
    "\"Language-Team: LANGUAGE <LL@li.org>\\n\"\n"
    "\"Language: \\n\"\n"
    "\"MIME-Version: 1.0\\n\"\n"
    "\"Content-Type: text/plain; charset=CHARSET\\n\"\n"
    "\"Content-Transfer-Encoding: 8bit\\n\"\n"
    "\n"
    "#. TRANSLATORS: greeting\n"
    "#: src/a.c:10\n"
    "#, c-format\n"
    "msgid \"Hello, %s\"\n"
    "msgstr \"\"\n"
    "\n"
    "#: src/b.c:20\n"
    "msgctxt \"menu\"\n"
    "msgid \"File\"\n"
    "msgid_plural \"Files\"\n"
    "msgstr[0] \"\"\n"
    "msgstr[1] \"\"\n";

/* ヘッダのないテンプレート */
static const char init_template_no_header[] =
    "#: src/a.c:10\n"
    "msgid \"Hello\"\n"
    "msgstr \"\"\n";

/* テスト用: text を読み込んで catalog に入れる */
static void read_catalog(const char *text, mnd_po_catalog_t *catalog)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    mnd_po_catalog_init(catalog);
    NAOS_REQUIRE_TRUE(mnd_po_read(text, strlen(text), "test.pot", catalog, &diags));
    NAOS_REQUIRE_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
}

/* テスト用: catalog を既定の設定で書き出して out に入れる */
static void write_catalog(const mnd_po_catalog_t *catalog, mnd_buf_t *out)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    mnd_buf_clear(out);
    NAOS_REQUIRE_TRUE(mnd_po_write(catalog, NULL, out, &diags));
    NAOS_REQUIRE_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
}

/* テスト用: header の項目 name の値が expected であることを確かめる */
static void check_field(const mnd_po_entry_t *header, const char *name, const char *expected)
{
    mnd_slice_t value;

    NAOS_REQUIRE_TRUE(mnd_po_header_get(header, name, &value));
    NAOS_CHECK_MEM_EQ(value.data, value.len, expected, strlen(expected));
}

NAOS_TEST(init_sets_language_date_and_clears_fuzzy)
{
    mnd_po_catalog_t catalog;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_po_entry_t *header;
    size_t index;

    read_catalog(init_template, &catalog);

    NAOS_REQUIRE_TRUE(mnd_po_init(&catalog, "ja", INIT_TEST_DATE, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));

    NAOS_REQUIRE_TRUE(mnd_po_catalog_header(&catalog, &index));
    NAOS_CHECK_UINT_EQ(0, index);
    header = &catalog.entries[index];
    check_field(header, "Language", "ja");
    check_field(header, "PO-Revision-Date", INIT_TEST_DATE);
    NAOS_CHECK_FALSE(mnd_po_flag_has(header, "fuzzy"));
    NAOS_CHECK_UINT_EQ(0, header->flags.count);

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(init_replaces_charset_placeholder_with_utf8)
{
    mnd_po_catalog_t catalog;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    size_t index;

    read_catalog(init_template, &catalog);

    NAOS_REQUIRE_TRUE(mnd_po_init(&catalog, "ja", INIT_TEST_DATE, &diags));
    NAOS_REQUIRE_TRUE(mnd_po_catalog_header(&catalog, &index));
    check_field(&catalog.entries[index], "Content-Type", "text/plain; charset=UTF-8");

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(init_replaces_charset_placeholder_case_insensitively)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_po_entry_t *header;
    static const char msgstr[] = "Content-Type: text/plain; Charset= charset ; format=flowed\n";

    header = mnd_po_catalog_append(&catalog);
    NAOS_REQUIRE_TRUE(header != NULL);
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&header->msgstr, msgstr, strlen(msgstr)));

    NAOS_REQUIRE_TRUE(mnd_po_init(&catalog, "fr", INIT_TEST_DATE, &diags));
    /* 値だけを置き換え、前後の空白と ; 以降は残す。ない項目は末尾に加える */
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[0].msgstr.items[0]),
                      "Content-Type: text/plain; Charset= UTF-8 ; format=flowed\n"
                      "Language: fr\n"
                      "PO-Revision-Date: " INIT_TEST_DATE "\n");

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(init_keeps_other_charset)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_po_entry_t *header;
    static const char msgstr[] = "Content-Type: text/plain; charset=utf-8\n"
                                 "Language: \n"
                                 "PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\n";

    header = mnd_po_catalog_append(&catalog);
    NAOS_REQUIRE_TRUE(header != NULL);
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&header->msgstr, msgstr, strlen(msgstr)));

    NAOS_REQUIRE_TRUE(mnd_po_init(&catalog, "de", INIT_TEST_DATE, &diags));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[0].msgstr.items[0]),
                      "Content-Type: text/plain; charset=utf-8\n"
                      "Language: de\n"
                      "PO-Revision-Date: " INIT_TEST_DATE "\n");

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(init_keeps_other_fields_comments_and_entries)
{
    mnd_po_catalog_t catalog;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_slice_t value;
    size_t index;

    read_catalog(init_template, &catalog);

    NAOS_REQUIRE_TRUE(mnd_po_init(&catalog, "ja", INIT_TEST_DATE, &diags));
    NAOS_REQUIRE_TRUE(mnd_po_catalog_header(&catalog, &index));
    NAOS_CHECK_FALSE(mnd_po_header_get(&catalog.entries[index], "Plural-Forms", &value));

    /* 変わるのは fuzzy の行と Language、PO-Revision-Date、charset の値だけ */
    write_catalog(&catalog, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "# SOME DESCRIPTIVE TITLE.\n"
                      "# Copyright (C) YEAR THE PACKAGE'S COPYRIGHT HOLDER\n"
                      "#\n"
                      "msgid \"\"\n"
                      "msgstr \"\"\n"
                      "\"Project-Id-Version: demo 1.0\\n\"\n"
                      "\"Report-Msgid-Bugs-To: \\n\"\n"
                      "\"POT-Creation-Date: 2026-01-02 03:04+0900\\n\"\n"
                      "\"PO-Revision-Date: " INIT_TEST_DATE "\\n\"\n"
                      "\"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n\"\n"
                      "\"Language-Team: LANGUAGE <LL@li.org>\\n\"\n"
                      "\"Language: ja\\n\"\n"
                      "\"MIME-Version: 1.0\\n\"\n"
                      "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                      "\"Content-Transfer-Encoding: 8bit\\n\"\n"
                      "\n"
                      "#. TRANSLATORS: greeting\n"
                      "#: src/a.c:10\n"
                      "#, c-format\n"
                      "msgid \"Hello, %s\"\n"
                      "msgstr \"\"\n"
                      "\n"
                      "#: src/b.c:20\n"
                      "msgctxt \"menu\"\n"
                      "msgid \"File\"\n"
                      "msgid_plural \"Files\"\n"
                      "msgstr[0] \"\"\n"
                      "msgstr[1] \"\"\n");

    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(init_removes_only_fuzzy_flag_from_header)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_po_entry_t *header;
    static const char msgstr[] = "Language: \n";

    header = mnd_po_catalog_append(&catalog);
    NAOS_REQUIRE_TRUE(header != NULL);
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(header, "fuzzy"));
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(header, "no-wrap"));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&header->msgstr, msgstr, strlen(msgstr)));

    NAOS_REQUIRE_TRUE(mnd_po_init(&catalog, "ja", INIT_TEST_DATE, &diags));
    NAOS_CHECK_FALSE(mnd_po_flag_has(&catalog.entries[0], "fuzzy"));
    NAOS_CHECK_TRUE(mnd_po_flag_has(&catalog.entries[0], "no-wrap"));
    NAOS_CHECK_UINT_EQ(1, catalog.entries[0].flags.count);

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(init_inserts_header_when_missing)
{
    mnd_po_catalog_t catalog;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    const mnd_po_entry_t *header;
    mnd_slice_t value;
    size_t index;

    read_catalog(init_template_no_header, &catalog);
    NAOS_REQUIRE_FALSE(mnd_po_catalog_header(&catalog, &index));

    NAOS_REQUIRE_TRUE(mnd_po_init(&catalog, "ja", INIT_TEST_DATE, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_UINT_EQ(2, catalog.count);
    NAOS_REQUIRE_TRUE(mnd_po_catalog_header(&catalog, &index));
    NAOS_CHECK_UINT_EQ(0, index);

    header = &catalog.entries[0];
    NAOS_CHECK_UINT_EQ(0, header->flags.count);
    NAOS_CHECK_UINT_EQ(0, header->translator_comments.count);
    NAOS_CHECK_FALSE(header->has_msgid_plural);
    NAOS_REQUIRE_UINT_EQ(1, header->msgstr.count);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&header->msgstr.items[0]),
                      "PO-Revision-Date: " INIT_TEST_DATE "\n"
                      "Language: ja\n"
                      "MIME-Version: 1.0\n"
                      "Content-Type: text/plain; charset=UTF-8\n"
                      "Content-Transfer-Encoding: 8bit\n");
    NAOS_CHECK_FALSE(mnd_po_header_get(header, "Plural-Forms", &value));

    write_catalog(&catalog, &out);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out),
                      "msgid \"\"\n"
                      "msgstr \"\"\n"
                      "\"PO-Revision-Date: " INIT_TEST_DATE "\\n\"\n"
                      "\"Language: ja\\n\"\n"
                      "\"MIME-Version: 1.0\\n\"\n"
                      "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                      "\"Content-Transfer-Encoding: 8bit\\n\"\n"
                      "\n"
                      "#: src/a.c:10\n"
                      "msgid \"Hello\"\n"
                      "msgstr \"\"\n");

    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

/* テスト用: language で失敗し、message の診断が 1 件だけ出て、catalog が変わらないことを確かめる */
static void check_rejected(const char *template_text, const char *language, const char *message)
{
    mnd_po_catalog_t catalog;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_buf_t before = MND_BUF_INIT;
    mnd_buf_t after = MND_BUF_INIT;
    size_t count;
    const mnd_diag_t *diag;

    read_catalog(template_text, &catalog);
    write_catalog(&catalog, &before);
    count = catalog.count;

    NAOS_CHECK_FALSE(mnd_po_init(&catalog, language, INIT_TEST_DATE, &diags));
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, diag->severity);
    NAOS_CHECK_STR_EQ(diag->message, message);

    NAOS_CHECK_UINT_EQ(count, catalog.count);
    write_catalog(&catalog, &after);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&after), mnd_buf_cstr(&before));

    mnd_buf_free(&before);
    mnd_buf_free(&after);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(init_rejects_empty_language)
{
    check_rejected(init_template, "", "invalid language code ''");
    check_rejected(init_template_no_header, "", "invalid language code ''");
}

NAOS_TEST(init_rejects_language_with_space)
{
    check_rejected(init_template, "ja JP", "invalid language code 'ja JP'");
    check_rejected(init_template_no_header, "ja JP", "invalid language code 'ja JP'");
}

NAOS_TEST(init_rejects_language_with_control_character)
{
    /* 表示では制御文字を ? に置き換える */
    check_rejected(init_template, "ja\t", "invalid language code 'ja?'");
    check_rejected(init_template_no_header, "ja\t", "invalid language code 'ja?'");
    check_rejected(init_template, "ja\x7f", "invalid language code 'ja?'");
    check_rejected(init_template, "ja\nJP", "invalid language code 'ja?JP'");
}

NAOS_TEST(init_rejected_language_is_shown_up_to_40_bytes)
{
    /* 50 バイトの値は先頭 40 バイトだけを表示する */
    check_rejected(init_template, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ",
                   "invalid language code 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'");
    /* 文字の途中では切らない（39 バイト + 3 バイトの文字） */
    check_rejected(init_template,
                   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\xe3\x81\x82 ",
                   "invalid language code 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'");
}

NAOS_TEST(init_accepts_language_with_region_and_modifier)
{
    mnd_po_catalog_t catalog;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    size_t index;

    read_catalog(init_template, &catalog);
    NAOS_REQUIRE_TRUE(mnd_po_init(&catalog, "sr_RS@latin", INIT_TEST_DATE, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_TRUE(mnd_po_catalog_header(&catalog, &index));
    check_field(&catalog.entries[index], "Language", "sr_RS@latin");

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(init,
    NAOS_ADD_TEST(init_sets_language_date_and_clears_fuzzy),
    NAOS_ADD_TEST(init_replaces_charset_placeholder_with_utf8),
    NAOS_ADD_TEST(init_replaces_charset_placeholder_case_insensitively),
    NAOS_ADD_TEST(init_keeps_other_charset),
    NAOS_ADD_TEST(init_keeps_other_fields_comments_and_entries),
    NAOS_ADD_TEST(init_removes_only_fuzzy_flag_from_header),
    NAOS_ADD_TEST(init_inserts_header_when_missing),
    NAOS_ADD_TEST(init_rejects_empty_language),
    NAOS_ADD_TEST(init_rejects_language_with_space),
    NAOS_ADD_TEST(init_rejects_language_with_control_character),
    NAOS_ADD_TEST(init_rejected_language_is_shown_up_to_40_bytes),
    NAOS_ADD_TEST(init_accepts_language_with_region_and_modifier)
)
