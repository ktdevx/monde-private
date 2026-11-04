/* Merge: 手順の実行、出力の順序、統計、エラー */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "po/po.h"
#include "po_merge/merge.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * catalog の末尾にエントリを加える。msgctxt が NULL なら msgctxt なし。
 * msgstr が NULL なら msgstr を持たない。line は入力上の行番号として設定する
 */
static void merge_test_add(mnd_po_catalog_t *catalog, const char *msgctxt, const char *msgid,
                           const char *msgstr, bool obsolete, size_t line)
{
    mnd_po_entry_t *entry = mnd_po_catalog_append(catalog);

    NAOS_REQUIRE_NOT_NULL(entry);
    if (msgctxt != NULL)
    {
        entry->has_msgctxt = true;
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgctxt, msgctxt));
    }

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid, msgid));
    if (msgstr != NULL)
    {
        NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry->msgstr, msgstr, strlen(msgstr)));
    }

    entry->obsolete = obsolete;
    entry->line = line;
}

/* 統計がすべて 0 であることを確かめる */
static void merge_test_check_stats_zero(const mnd_merge_stats_t *stats)
{
    NAOS_CHECK_UINT_EQ(stats->matched, 0);
    NAOS_CHECK_UINT_EQ(stats->fuzzy, 0);
    NAOS_CHECK_UINT_EQ(stats->added, 0);
    NAOS_CHECK_UINT_EQ(stats->obsoleted, 0);
}

/* 診断 index の重大度、メッセージ、path、行番号を確かめる */
static void merge_test_check_diag(const mnd_diag_list_t *diags, size_t index, const char *message,
                                  const char *path, size_t line)
{
    const mnd_diag_t *diag;

    NAOS_REQUIRE_UINT_GT(mnd_diag_list_count(diags), index);
    diag = mnd_diag_list_at(diags, index);
    NAOS_CHECK_INT_EQ(diag->severity, MND_SEVERITY_ERROR);
    NAOS_CHECK_STR_EQ(diag->message, message);
    if (path == NULL)
    {
        NAOS_CHECK_NULL(diag->path);
    }
    else
    {
        NAOS_REQUIRE_NOT_NULL(diag->path);
        NAOS_CHECK_STR_EQ(diag->path, path);
    }

    NAOS_CHECK_UINT_EQ(diag->line, line);
}

/* 同じキーのエントリを 2 つ（行 3 と 6）持つテンプレートで、報告が 1 つで expected であることを確かめる */
static void merge_test_duplicate_message(const char *msgctxt, const char *msgid,
                                         const char *expected)
{
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_diag_list_t diags;

    mnd_po_catalog_init(&template_catalog);
    mnd_po_catalog_init(&old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);
    merge_test_add(&template_catalog, msgctxt, msgid, "", false, 3);
    merge_test_add(&template_catalog, msgctxt, msgid, "", false, 6);

    NAOS_CHECK_FALSE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                                        &out, NULL, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 1);
    merge_test_check_diag(&diags, 0, expected, "t.pot", 6);
    NAOS_CHECK_UINT_EQ(out.count, 0);

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

/* 出力に、msgctxt のない空の msgid の廃止エントリでないエントリ（ヘッダの形）があれば真 */
static bool merge_test_has_header_shape(const mnd_po_catalog_t *catalog)
{
    size_t i;

    for (i = 0; i < catalog->count; i++)
    {
        const mnd_po_entry_t *entry = &catalog->entries[i];

        if ((!entry->obsolete) && (!entry->has_msgctxt) && (entry->msgid.len == 0))
        {
            return true;
        }
    }

    return false;
}

NAOS_TEST(merge_duplicates_in_both_inputs_are_all_reported)
{
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_merge_stats_t stats = { 1, 2, 3, 4 };
    mnd_diag_list_t diags;
    bool ok;

    mnd_po_catalog_init(&template_catalog);
    mnd_po_catalog_init(&old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);
    merge_test_add(&template_catalog, NULL, "", "", false, 1);
    merge_test_add(&template_catalog, NULL, "a", "", false, 5);
    merge_test_add(&template_catalog, "menu", "b", "", false, 8);
    merge_test_add(&template_catalog, NULL, "a", "", false, 12);
    merge_test_add(&template_catalog, "menu", "b", "", false, 15);
    merge_test_add(&old_catalog, NULL, "", "Content-Type: text/plain; charset=UTF-8\n", false, 1);
    merge_test_add(&old_catalog, NULL, "c", "C", false, 4);
    merge_test_add(&old_catalog, NULL, "c", "C2", false, 9);
    merge_test_add(&old_catalog, NULL, "c", "C3", false, 20);

    /* 両方の入力の重複を、後のエントリについてすべて報告する */
    ok = mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                            &out, &stats, &diags);
    NAOS_CHECK_FALSE(ok);
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 4);
    merge_test_check_diag(&diags, 0,
                          "duplicate message definition: msgid \"a\" (first defined at line 5)",
                          "t.pot", 12);
    merge_test_check_diag(&diags, 1,
                          "duplicate message definition: msgctxt \"menu\", msgid \"b\""
                          " (first defined at line 8)",
                          "t.pot", 15);
    merge_test_check_diag(&diags, 2,
                          "duplicate message definition: msgid \"c\" (first defined at line 4)",
                          "o.po", 9);
    merge_test_check_diag(&diags, 3,
                          "duplicate message definition: msgid \"c\" (first defined at line 4)",
                          "o.po", 20);

    /* 出力は空、統計は 0 */
    NAOS_CHECK_UINT_EQ(out.count, 0);
    merge_test_check_stats_zero(&stats);

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_TEST(merge_duplicate_without_line_numbers)
{
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_merge_stats_t stats = { 1, 1, 1, 1 };
    mnd_diag_list_t diags;

    mnd_po_catalog_init(&template_catalog);
    mnd_po_catalog_init(&old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);
    merge_test_add(&old_catalog, "", "x", "X", false, 0);
    merge_test_add(&old_catalog, "", "x", "X", false, 0);

    /* 先の行番号が 0 なら括弧の部分を付けず、行番号も 0、path が NULL なら NULL */
    NAOS_CHECK_FALSE(mnd_merge_catalogs(&template_catalog, NULL, &old_catalog, NULL, NULL,
                                        &out, &stats, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 1);
    merge_test_check_diag(&diags, 0, "duplicate message definition: msgctxt \"\", msgid \"x\"",
                          NULL, 0);
    NAOS_CHECK_UINT_EQ(out.count, 0);
    merge_test_check_stats_zero(&stats);

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_TEST(merge_duplicate_message_truncates_and_replaces_control_chars)
{
    /* 40 バイトちょうどは切らない */
    merge_test_duplicate_message(NULL, "0123456789012345678901234567890123456789",
                                 "duplicate message definition: msgid "
                                 "\"0123456789012345678901234567890123456789\""
                                 " (first defined at line 3)");
    /* 41 バイトは 40 バイトに切って ... を付ける */
    merge_test_duplicate_message(NULL, "0123456789012345678901234567890123456789X",
                                 "duplicate message definition: msgid "
                                 "\"0123456789012345678901234567890123456789...\""
                                 " (first defined at line 3)");
    /* UTF-8 の文字（3 バイト）の途中で切らない: 39 バイト + 「あ」は 39 バイトで切る */
    merge_test_duplicate_message(NULL, "012345678901234567890123456789012345678\xE3\x81\x82",
                                 "duplicate message definition: msgid "
                                 "\"012345678901234567890123456789012345678...\""
                                 " (first defined at line 3)");
    /* 37 バイト + 「あ」はちょうど 40 バイトで切らない */
    merge_test_duplicate_message(NULL, "0123456789012345678901234567890123456\xE3\x81\x82",
                                 "duplicate message definition: msgid "
                                 "\"0123456789012345678901234567890123456\xE3\x81\x82\""
                                 " (first defined at line 3)");
    /* 制御文字（C0、DEL、C1）は ? に置き換える。msgctxt も同じ規則 */
    merge_test_duplicate_message("c\x01tx", "a\tb\nc\x7F" "d\xC2\x85" "e",
                                 "duplicate message definition: msgctxt \"c?tx\", msgid \"a?b?c?d?e\""
                                 " (first defined at line 3)");
    /* 書式の指定子は値としてそのまま表示する */
    merge_test_duplicate_message("%s%n", "%d%%",
                                 "duplicate message definition: msgctxt \"%s%n\", msgid \"%d%%\""
                                 " (first defined at line 3)");
    /* msgctxt も 40 バイトで切る */
    merge_test_duplicate_message("abcdefghijabcdefghijabcdefghijabcdefghijK", "m",
                                 "duplicate message definition: msgctxt "
                                 "\"abcdefghijabcdefghijabcdefghijabcdefghij...\", msgid \"m\""
                                 " (first defined at line 3)");
}

NAOS_TEST(merge_obsolete_duplicates_are_not_reported)
{
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_merge_stats_t stats = { 1, 1, 1, 1 };
    mnd_diag_list_t diags;

    mnd_po_catalog_init(&template_catalog);
    mnd_po_catalog_init(&old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);
    /* 廃止エントリどうし、廃止エントリと通常のエントリ。msgctxt のない・空の msgctxt は別のキー */
    merge_test_add(&template_catalog, NULL, "t", "", false, 1);
    merge_test_add(&template_catalog, NULL, "t", "", true, 2);
    merge_test_add(&template_catalog, NULL, "t", "", true, 3);
    merge_test_add(&template_catalog, "", "t", "", false, 4);
    merge_test_add(&old_catalog, NULL, "x", "X", true, 1);
    merge_test_add(&old_catalog, NULL, "x", "X", false, 2);
    merge_test_add(&old_catalog, NULL, "y", "Y", true, 3);
    merge_test_add(&old_catalog, NULL, "y", "Y", true, 4);
    merge_test_add(&old_catalog, "ctx", "x", "X", false, 5);

    NAOS_CHECK_TRUE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                                       &out, &stats, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_TEST(merge_header_from_old_only)
{
    static const char template_po[] =
        "msgid \"a\"\n"
        "msgstr \"\"\n";
    static const char old_po[] =
        "# Translator comment\n"
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Project-Id-Version: demo 1.0\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n";
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_diag_list_t diags;
    const mnd_po_entry_t *header;

    TEST_READ_PO(template_po, &template_catalog);
    TEST_READ_PO(old_po, &old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);

    /* 既存だけにヘッダがあれば、既存のヘッダ（テンプレートのヘッダは NULL）で作って先頭に置く */
    NAOS_REQUIRE_TRUE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                                         &out, NULL, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_GT(out.count, 0);
    header = &out.entries[0];
    NAOS_CHECK_FALSE(header->has_msgctxt);
    NAOS_CHECK_UINT_EQ(header->msgid.len, 0);
    NAOS_CHECK_FALSE(header->obsolete);
    NAOS_CHECK_UINT_EQ(header->line, 0);
    NAOS_REQUIRE_UINT_EQ(header->msgstr.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&header->msgstr.items[0]),
                      "Project-Id-Version: demo 1.0\n"
                      "Content-Type: text/plain; charset=UTF-8\n");
    NAOS_REQUIRE_UINT_EQ(header->translator_comments.count, 1);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&header->translator_comments.items[0]), "Translator comment");

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_TEST(merge_header_from_both_is_merged)
{
    static const char template_po[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Report-Msgid-Bugs-To: bugs@example.com\\n\"\n"
        "\"POT-Creation-Date: 2026-01-02 03:04+0000\\n\"\n";
    static const char old_po[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Project-Id-Version: demo 1.0\\n\"\n"
        "\"POT-Creation-Date: 2025-01-01 00:00+0000\\n\"\n";
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_diag_list_t diags;
    mnd_merge_stats_t stats = { 1, 1, 1, 1 };

    TEST_READ_PO(template_po, &template_catalog);
    TEST_READ_PO(old_po, &old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);

    /* 両方にヘッダがあり、ほかのエントリがなければ、出力はマージしたヘッダだけ。統計は 0 */
    NAOS_REQUIRE_TRUE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                                         &out, &stats, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    TEST_CHECK_WRITTEN(&out,
                       "msgid \"\"\n"
                       "msgstr \"\"\n"
                       "\"Project-Id-Version: demo 1.0\\n\"\n"
                       "\"Report-Msgid-Bugs-To: bugs@example.com\\n\"\n"
                       "\"POT-Creation-Date: 2026-01-02 03:04+0000\\n\"\n");
    merge_test_check_stats_zero(&stats);

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_TEST(merge_header_from_template_only_is_not_output)
{
    static const char template_po[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"a\"\n"
        "msgstr \"\"\n";
    static const char old_po[] =
        "msgid \"a\"\n"
        "msgstr \"A\"\n";
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_diag_list_t diags;

    TEST_READ_PO(template_po, &template_catalog);
    TEST_READ_PO(old_po, &old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);

    /* 既存にヘッダがなければ、テンプレートのヘッダは無視してヘッダを出力しない */
    NAOS_REQUIRE_TRUE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                                         &out, NULL, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_CHECK_FALSE(merge_test_has_header_shape(&out));

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_TEST(merge_header_absent_in_both_is_not_output)
{
    static const char template_po[] =
        "msgid \"a\"\n"
        "msgstr \"\"\n";
    static const char old_po[] =
        "msgid \"b\"\n"
        "msgstr \"B\"\n";
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_diag_list_t diags;

    TEST_READ_PO(template_po, &template_catalog);
    TEST_READ_PO(old_po, &old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);

    /* どちらにもヘッダがなければ、ヘッダを出力しない */
    NAOS_REQUIRE_TRUE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                                         &out, NULL, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_CHECK_FALSE(merge_test_has_header_shape(&out));

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_TEST(merge_obsolete_header_of_old_is_not_a_header)
{
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_diag_list_t diags;

    mnd_po_catalog_init(&template_catalog);
    mnd_po_catalog_init(&old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);
    merge_test_add(&old_catalog, NULL, "", "Content-Type: text/plain; charset=UTF-8\n", true, 1);

    /* 既存の廃止エントリの空の msgid はヘッダではないので、ヘッダを出力しない */
    NAOS_REQUIRE_TRUE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                                         &out, NULL, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_CHECK_FALSE(merge_test_has_header_shape(&out));

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_TEST(merge_scaffold_test_util_round_trips_po)
{
    static const char po[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"Plural-Forms: nplurals=1; plural=0;\\n\"\n"
        "\n"
        "#. TRANSLATORS: greeting\n"
        "#: src/a.c:10\n"
        "#, c-format\n"
        "msgid \"Hello, %s\"\n"
        "msgstr \"こんにちは、%s\"\n"
        "\n"
        "msgctxt \"menu\"\n"
        "msgid \"File\"\n"
        "msgstr \"\"\n"
        "\n"
        "#~ msgid \"Old\"\n"
        "#~ msgstr \"古い\"\n";
    mnd_po_catalog_t catalog;

    /* 補助関数で読み込んで書き出すと元に戻る */
    TEST_READ_PO(po, &catalog);
    NAOS_CHECK_UINT_EQ(catalog.count, 4);
    TEST_CHECK_WRITTEN(&catalog, po);

    mnd_po_catalog_free(&catalog);
}

NAOS_SUITE(merge,
    NAOS_ADD_TEST(merge_duplicates_in_both_inputs_are_all_reported),
    NAOS_ADD_TEST(merge_duplicate_without_line_numbers),
    NAOS_ADD_TEST(merge_duplicate_message_truncates_and_replaces_control_chars),
    NAOS_ADD_TEST(merge_obsolete_duplicates_are_not_reported),
    NAOS_ADD_TEST(merge_header_from_old_only),
    NAOS_ADD_TEST(merge_header_from_both_is_merged),
    NAOS_ADD_TEST(merge_header_from_template_only_is_not_output),
    NAOS_ADD_TEST(merge_header_absent_in_both_is_not_output),
    NAOS_ADD_TEST(merge_obsolete_header_of_old_is_not_a_header),
    NAOS_ADD_TEST(merge_scaffold_test_util_round_trips_po)
)
