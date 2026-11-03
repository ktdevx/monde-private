#include <naos/naos.h>

#include "foundation/diag.h"
#include "po/po_read.h"

#include <string.h>

#define READ_TEST_PATH "test.po"

/* テスト用: NUL 終端の内容を読み込む */
static bool read_str(const char *content, mnd_po_catalog_t *catalog, mnd_diag_list_t *diags)
{
    return mnd_po_read(content, strlen(content), READ_TEST_PATH, catalog, diags);
}

/* テスト用: 1 件だけのエラーの診断のメッセージと行番号を確かめる */
static void check_single_error(const mnd_diag_list_t *diags, const char *message, size_t line)
{
    const mnd_diag_t *diag;

    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(diags), 1);
    diag = mnd_diag_list_at(diags, 0);
    NAOS_CHECK_INT_EQ(diag->severity, MND_SEVERITY_ERROR);
    NAOS_CHECK_STR_EQ(diag->message, message);
    NAOS_REQUIRE_NOT_NULL(diag->path);
    NAOS_CHECK_STR_EQ(diag->path, READ_TEST_PATH);
    NAOS_CHECK_UINT_EQ(diag->line, line);
}

/* テスト用: 1 行の誤りのある内容が、そのメッセージと行番号で失敗し、カタログが空になる */
static void check_read_error(const char *content, const char *message, size_t line)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_FALSE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    check_single_error(&diags, message, line);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_output_catalog_starts_empty)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_UINT_EQ(0, catalog.count);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_comments_of_each_kind)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_po_entry_t *entry;
    const char *content =
        "# translator one\n"
        "#  two spaces\n"
        "#\n"
        "#no space\n"
        "#\ttab\n"
        "  # leading blanks\n"
        "#. extracted\n"
        "#.\n"
        "#.tight\n"
        "#: src/a.c:12\n"
        "#, fuzzy, c-format\n"
        "#| msgctxt \"old ctx\"\n"
        "#| msgid \"old\"\n"
        "#| \"er\"\n"
        "#| msgid_plural \"olds\"\n"
        "msgctxt \"ctx\"\n"
        "msgid \"hello\"\n"
        "msgstr \"hi\"\n";

    NAOS_REQUIRE_TRUE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(catalog.count, 1);
    entry = &catalog.entries[0];

    NAOS_REQUIRE_UINT_EQ(entry->translator_comments.count, 6);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->translator_comments.items[0]), "translator one");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->translator_comments.items[1]), " two spaces");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->translator_comments.items[2]), "");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->translator_comments.items[3]), "no space");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->translator_comments.items[4]), "\ttab");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->translator_comments.items[5]), "leading blanks");

    NAOS_REQUIRE_UINT_EQ(entry->extracted_comments.count, 3);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->extracted_comments.items[0]), "extracted");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->extracted_comments.items[1]), "");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->extracted_comments.items[2]), "tight");

    NAOS_REQUIRE_UINT_EQ(entry->refs.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->refs.items[0].file), "src/a.c");
    NAOS_CHECK_UINT_EQ(entry->refs.items[0].line, 12);

    NAOS_REQUIRE_UINT_EQ(entry->flags.count, 2);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->flags.items[0]), "fuzzy");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->flags.items[1]), "c-format");

    NAOS_CHECK_TRUE(entry->has_prev_msgctxt);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->prev_msgctxt), "old ctx");
    NAOS_CHECK_TRUE(entry->has_prev_msgid);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->prev_msgid), "older");
    NAOS_CHECK_TRUE(entry->has_prev_msgid_plural);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->prev_msgid_plural), "olds");

    NAOS_CHECK_TRUE(entry->has_msgctxt);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->msgctxt), "ctx");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->msgid), "hello");
    NAOS_REQUIRE_UINT_EQ(entry->msgstr.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->msgstr.items[0]), "hi");
    NAOS_CHECK_FALSE(entry->obsolete);
    NAOS_CHECK_UINT_EQ(entry->line, 1);

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_references_with_and_without_line_numbers)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_po_refs_t *refs;
    const char *content =
        "#: a.c:1 b.c\tc.c:x  d:e:10\n"
        "#: lib/f.c: g.c:0 :5\n"
        "msgid \"x\"\n"
        "msgstr \"\"\n";

    NAOS_REQUIRE_TRUE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(catalog.count, 1);
    refs = &catalog.entries[0].refs;
    NAOS_REQUIRE_UINT_EQ(refs->count, 7);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[0].file), "a.c");
    NAOS_CHECK_UINT_EQ(refs->items[0].line, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[1].file), "b.c");
    NAOS_CHECK_UINT_EQ(refs->items[1].line, 0);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[2].file), "c.c:x");
    NAOS_CHECK_UINT_EQ(refs->items[2].line, 0);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[3].file), "d:e");
    NAOS_CHECK_UINT_EQ(refs->items[3].line, 10);
    /* ':' の後ろに数字がなければ、語全体がファイル名 */
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[4].file), "lib/f.c:");
    NAOS_CHECK_UINT_EQ(refs->items[4].line, 0);
    /* 行番号 0 は「行番号なし」と区別できないため、語全体をファイル名とする */
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[5].file), "g.c:0");
    NAOS_CHECK_UINT_EQ(refs->items[5].line, 0);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[6].file), "");
    NAOS_CHECK_UINT_EQ(refs->items[6].line, 5);

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_references_isolated_file_names)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_po_refs_t *refs;
    /* U+2068 = E2 81 A8、U+2069 = E2 81 A9 */
    const char *content =
        "#: \xE2\x81\xA8my file.c\xE2\x81\xA9:42 "
        "\xE2\x81\xA8other dir/x\ty.c\xE2\x81\xA9 plain.c:3 "
        "\xE2\x81\xA8" "a:1" "\xE2\x81\xA9\n"
        "msgid \"x\"\n"
        "msgstr \"\"\n";

    NAOS_REQUIRE_TRUE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(catalog.count, 1);
    refs = &catalog.entries[0].refs;
    NAOS_REQUIRE_UINT_EQ(refs->count, 4);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[0].file), "my file.c");
    NAOS_CHECK_UINT_EQ(refs->items[0].line, 42);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[1].file), "other dir/x\ty.c");
    NAOS_CHECK_UINT_EQ(refs->items[1].line, 0);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[2].file), "plain.c");
    NAOS_CHECK_UINT_EQ(refs->items[2].line, 3);
    /* 囲まれた中の ':' と数字は行番号にしない */
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&refs->items[3].file), "a:1");
    NAOS_CHECK_UINT_EQ(refs->items[3].line, 0);

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_flags_split_and_trimmed)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_po_strings_t *flags;
    const char *content =
        "#,  fuzzy ,c-format,,\tno-wrap\t,\n"
        "#, python-format, fuzzy\n"
        "#,\n"
        "msgid \"x\"\n"
        "msgstr \"\"\n";

    NAOS_REQUIRE_TRUE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(catalog.count, 1);
    flags = &catalog.entries[0].flags;
    NAOS_REQUIRE_UINT_EQ(flags->count, 4);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&flags->items[0]), "fuzzy");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&flags->items[1]), "c-format");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&flags->items[2]), "no-wrap");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&flags->items[3]), "python-format");

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_string_literals)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_po_entry_t *entry;
    const char *content =
        "\tmsgid \"a\\n\\t\\\"q\\\" \\\\ \\101\\x42\"  \t\n"
        "msgstr  \"\xE3\x81\x82\"\n";

    NAOS_REQUIRE_TRUE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(catalog.count, 1);
    entry = &catalog.entries[0];
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->msgid), "a\n\t\"q\" \\ AB");
    NAOS_REQUIRE_UINT_EQ(entry->msgstr.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&entry->msgstr.items[0]), "\xE3\x81\x82");
    NAOS_CHECK_FALSE(entry->has_msgctxt);

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_mixed_crlf_and_lf)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const char *content =
        "# c\r\n"
        "#: a.c:3\r\n"
        "#, fuzzy\r\n"
        "msgid \"a\"\n"
        "msgstr \"b\"\r\n"
        "\r\n"
        "msgid \"c\"\r\n"
        "\"d\"\n"
        "msgstr \"\"\r\n";

    NAOS_REQUIRE_TRUE(read_str(content, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(catalog.count, 2);

    NAOS_REQUIRE_UINT_EQ(catalog.entries[0].translator_comments.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[0].translator_comments.items[0]), "c");
    NAOS_REQUIRE_UINT_EQ(catalog.entries[0].refs.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[0].refs.items[0].file), "a.c");
    NAOS_CHECK_UINT_EQ(catalog.entries[0].refs.items[0].line, 3);
    NAOS_REQUIRE_UINT_EQ(catalog.entries[0].flags.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[0].flags.items[0]), "fuzzy");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[0].msgid), "a");
    NAOS_REQUIRE_UINT_EQ(catalog.entries[0].msgstr.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[0].msgstr.items[0]), "b");
    NAOS_CHECK_UINT_EQ(catalog.entries[0].line, 1);

    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[1].msgid), "cd");
    NAOS_REQUIRE_UINT_EQ(catalog.entries[1].msgstr.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[1].msgstr.items[0]), "");
    NAOS_CHECK_UINT_EQ(catalog.entries[1].line, 7);

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_skips_leading_bom)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    /* BOM の直後のキーワードも認識する */
    NAOS_REQUIRE_TRUE(read_str("\xEF\xBB\xBFmsgid \"\"\nmsgstr \"x\"\n", &catalog, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(catalog.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[0].msgid), "");
    NAOS_CHECK_UINT_EQ(catalog.entries[0].line, 1);
    mnd_po_catalog_free(&catalog);

    /* BOM はコメントの内容に含めない */
    mnd_po_catalog_init(&catalog);
    NAOS_REQUIRE_TRUE(read_str("\xEF\xBB\xBF# c\nmsgid \"a\"\nmsgstr \"b\"\n", &catalog, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(catalog.count, 1);
    NAOS_REQUIRE_UINT_EQ(catalog.entries[0].translator_comments.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[0].translator_comments.items[0]), "c");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&catalog.entries[0].msgid), "a");

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_bom_only_and_empty_content)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_TRUE(mnd_po_read(NULL, 0, READ_TEST_PATH, &catalog, &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    NAOS_CHECK_TRUE(read_str("\xEF\xBB\xBF", &catalog, &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);

    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_invalid_utf8_reports_line_and_does_not_parse)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    /* 3 行目の不正なバイト。その後ろの未知のキーワードは解析しないため報告しない */
    const char content[] = "msgid \"a\"\r\nmsgstr \"\"\n# \xFF\nbogus\n";

    NAOS_CHECK_FALSE(mnd_po_read(content, sizeof(content) - 1, READ_TEST_PATH, &catalog,
                                 &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    check_single_error(&diags, "invalid UTF-8 sequence", 3);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);

    /* BOM の後ろの 1 行目 */
    check_read_error("\xEF\xBB\xBF\xC3(\n", "invalid UTF-8 sequence", 1);
}

NAOS_TEST(read_literal_errors)
{
    check_read_error("msgid \"abc\nmsgstr \"\"\n", "unterminated string", 1);
    check_read_error("msgid \"a\"\nmsgstr \"abc\\\"\n", "unterminated string", 2);
    check_read_error("msgid \"a\" x\nmsgstr \"\"\n", "unexpected text after string", 1);
    check_read_error("msgid \"a\"\nmsgstr \"\"\n\"b\"\"c\"\n", "unexpected text after string",
                     3);
    check_read_error("msgid\nmsgstr \"\"\n", "missing string after 'msgid'", 1);
    check_read_error("msgid \"a\"\nmsgstr[0]   \n", "missing string after 'msgstr[0]'", 2);
    check_read_error("msgid \"a\"\nmsgstr x\n", "missing string after 'msgstr'", 2);
    check_read_error("#| msgid\nmsgid \"a\"\nmsgstr \"\"\n", "missing string after 'msgid'", 1);
}

NAOS_TEST(read_escape_errors)
{
    check_read_error("msgid \"a\\q\"\nmsgstr \"\"\n", "invalid escape sequence '\\q'", 1);
    check_read_error("msgid \"a\\x1ff\"\nmsgstr \"\"\n", "invalid escape sequence '\\x'", 1);
    check_read_error("msgid \"a\\\xE3\x81\x82\"\nmsgstr \"\"\n",
                     "invalid escape sequence '\\\xE3\x81\x82'", 1);
    check_read_error("msgid \"a\"\nmsgstr \"\\0\"\n", "string contains a NUL byte", 2);
    check_read_error("msgid \"a\"\nmsgstr \"\\xff\"\n", "invalid UTF-8 sequence", 2);
}

NAOS_TEST(read_unknown_keyword)
{
    check_read_error("msgid \"a\"\nmsgstr \"\"\nmsgfoo \"x\"\n", "unknown keyword 'msgfoo'", 3);
    check_read_error("msgid\"a\"\nmsgstr \"\"\n", "unknown keyword 'msgid\"a\"'", 1);
    check_read_error("msgstr[x] \"a\"\n", "unknown keyword 'msgstr[x]'", 1);
    check_read_error("#| msgstr \"a\"\nmsgid \"a\"\nmsgstr \"\"\n", "unknown keyword 'msgstr'",
                     1);
    /* 40 バイトまでに切り詰め、制御文字を '?' に置き換える */
    check_read_error("abc\001defghijklmnopqrstuvwxyz0123456789ABCDEFGH \"x\"\n",
                     "unknown keyword 'abc?defghijklmnopqrstuvwxyz0123456789ABC'", 1);
}

NAOS_TEST(read_raw_nul_in_comment)
{
    const char content[] = "# a\0b\nmsgid \"a\"\nmsgstr \"\"\n";
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_FALSE(mnd_po_read(content, sizeof(content) - 1, READ_TEST_PATH, &catalog,
                                 &diags));
    NAOS_CHECK_UINT_EQ(catalog.count, 0);
    check_single_error(&diags, "string contains a NUL byte", 1);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(read_path_may_be_null)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const char *content = "msgid \"a\" x\n";

    NAOS_CHECK_FALSE(mnd_po_read(content, strlen(content), NULL, &catalog, &diags));
    NAOS_REQUIRE_UINT_EQ(mnd_diag_list_count(&diags), 1);
    NAOS_CHECK_NULL(mnd_diag_list_at(&diags, 0)->path);
    NAOS_CHECK_UINT_EQ(mnd_diag_list_at(&diags, 0)->line, 1);
    mnd_po_catalog_free(&catalog);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(read,
    NAOS_ADD_TEST(read_output_catalog_starts_empty),
    NAOS_ADD_TEST(read_comments_of_each_kind),
    NAOS_ADD_TEST(read_references_with_and_without_line_numbers),
    NAOS_ADD_TEST(read_references_isolated_file_names),
    NAOS_ADD_TEST(read_flags_split_and_trimmed),
    NAOS_ADD_TEST(read_string_literals),
    NAOS_ADD_TEST(read_mixed_crlf_and_lf),
    NAOS_ADD_TEST(read_skips_leading_bom),
    NAOS_ADD_TEST(read_bom_only_and_empty_content),
    NAOS_ADD_TEST(read_invalid_utf8_reports_line_and_does_not_parse),
    NAOS_ADD_TEST(read_literal_errors),
    NAOS_ADD_TEST(read_escape_errors),
    NAOS_ADD_TEST(read_unknown_keyword),
    NAOS_ADD_TEST(read_raw_nul_in_comment),
    NAOS_ADD_TEST(read_path_may_be_null)
)
