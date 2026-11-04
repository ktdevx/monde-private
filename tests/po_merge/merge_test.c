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

/* テンプレートの、廃止エントリでもヘッダでもないエントリの数 */
static size_t merge_test_count_messages(const mnd_po_catalog_t *catalog)
{
    size_t header_index = 0;
    bool has_header = mnd_po_catalog_header(catalog, &header_index);
    size_t count = 0;
    size_t i;

    for (i = 0; i < catalog->count; i++)
    {
        if (catalog->entries[i].obsolete || (has_header && (i == header_index)))
        {
            continue;
        }

        count++;
    }

    return count;
}

/*
 * template_po と old_po（NULL なら空のカタログ）を読み込んで options（NULL なら既定）でマージし、
 * 書き出した結果が expected と一致すること、統計が expected_stats と一致し matched + fuzzy + added がテンプレートの
 * メッセージの数に等しいこと、診断がないこと、入力のカタログがマージの前後で変わらないこと
 * （書き出しが入力の文字列と一致すること）を確かめる。入力の文字列は書き出しの正規形で書く
 */
static void merge_test_run_options(const mnd_merge_options_t *options, const char *template_po,
                                   const char *old_po, const char *expected,
                                   const mnd_merge_stats_t *expected_stats)
{
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_merge_stats_t stats = { 9, 9, 9, 9 };
    mnd_diag_list_t diags;

    TEST_READ_PO(template_po, &template_catalog);
    if (old_po != NULL)
    {
        TEST_READ_PO(old_po, &old_catalog);
    }
    else
    {
        mnd_po_catalog_init(&old_catalog);
    }

    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);

    /* マージの前の入力の書き出し */
    TEST_CHECK_WRITTEN(&template_catalog, template_po);
    TEST_CHECK_WRITTEN(&old_catalog, (old_po != NULL) ? old_po : "");

    NAOS_REQUIRE_TRUE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po",
                                         options, &out, &stats, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);

    /* 結果は mnd_po_write で重複のエラーなく書き出せる（診断があれば失敗する） */
    TEST_CHECK_WRITTEN(&out, expected);
    NAOS_CHECK_UINT_EQ(stats.matched, expected_stats->matched);
    NAOS_CHECK_UINT_EQ(stats.fuzzy, expected_stats->fuzzy);
    NAOS_CHECK_UINT_EQ(stats.added, expected_stats->added);
    NAOS_CHECK_UINT_EQ(stats.obsoleted, expected_stats->obsoleted);
    NAOS_CHECK_UINT_EQ(stats.matched + stats.fuzzy + stats.added,
                       merge_test_count_messages(&template_catalog));

    /* 入力のカタログはマージの後も変わらない */
    TEST_CHECK_WRITTEN(&template_catalog, template_po);
    TEST_CHECK_WRITTEN(&old_catalog, (old_po != NULL) ? old_po : "");

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

/* 既定の設定（options が NULL）で merge_test_run_options を行う */
static void merge_test_run(const char *template_po, const char *old_po, const char *expected,
                           const mnd_merge_stats_t *expected_stats)
{
    merge_test_run_options(NULL, template_po, old_po, expected, expected_stats);
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

NAOS_TEST(merge_output_order_and_template_obsolete_ignored)
{
    /* 既存のヘッダは先頭でなくてもよい */
    static const char template_po[] =
        "msgid \"\"\n"
        "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"c\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"a\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"b\"\n"
        "msgstr \"\"\n"
        "\n"
        "#~ msgid \"x\"\n"
        "#~ msgstr \"\"\n"
        "\n"
        "#~ msgid \"q\"\n"
        "#~ msgstr \"\"\n";
    static const char old_po[] =
        "msgid \"x\"\n"
        "msgstr \"X\"\n"
        "\n"
        "#~ msgid \"z\"\n"
        "#~ msgstr \"Z\"\n"
        "\n"
        "msgid \"\"\n"
        "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"a\"\n"
        "msgstr \"A\"\n"
        "\n"
        "msgid \"y\"\n"
        "msgstr \"\"\n"
        "\n"
        "#~ msgid \"w\"\n"
        "#~ msgstr \"W\"\n";
    static const mnd_merge_stats_t stats = { 1, 0, 2, 1 };

    /*
     * ヘッダ、テンプレートの順（テンプレートの廃止エントリは飛ばす）、既存の順の廃止エントリ。
     * テンプレートの廃止エントリ x は既存の x を使わないので、x は廃止エントリになる。
     * 訳のない y は出力しない。元からの廃止エントリ z と w は obsoleted に数えない
     */
    merge_test_run(template_po, old_po,
                   "msgid \"\"\n"
                   "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                   "\n"
                   "msgid \"c\"\n"
                   "msgstr \"\"\n"
                   "\n"
                   "msgid \"a\"\n"
                   "msgstr \"A\"\n"
                   "\n"
                   "msgid \"b\"\n"
                   "msgstr \"\"\n"
                   "\n"
                   "#~ msgid \"x\"\n"
                   "#~ msgstr \"X\"\n"
                   "\n"
                   "#~ msgid \"z\"\n"
                   "#~ msgstr \"Z\"\n"
                   "\n"
                   "#~ msgid \"w\"\n"
                   "#~ msgstr \"W\"\n",
                   &stats);
}

NAOS_TEST(merge_output_is_one_to_one_with_template)
{
    static const char template_po[] =
        "msgid \"\"\n"
        "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"a\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgctxt \"m\"\n"
        "msgid \"a\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"n\"\n"
        "msgstr \"\"\n"
        "\n"
        "#~ msgid \"o\"\n"
        "#~ msgstr \"\"\n";
    static const char old_po[] =
        "msgid \"\"\n"
        "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"a\"\n"
        "msgstr \"A\"\n"
        "\n"
        "msgctxt \"m\"\n"
        "msgid \"a\"\n"
        "msgstr \"MA\"\n";
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_diag_list_t diags;
    size_t i;

    TEST_READ_PO(template_po, &template_catalog);
    TEST_READ_PO(old_po, &old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);

    /* ヘッダ 1 つ + テンプレートのメッセージ 3 つ。各エントリの行番号は 0 */
    NAOS_REQUIRE_TRUE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                                         &out, NULL, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(out.count, 4);
    for (i = 0; i < out.count; i++)
    {
        NAOS_CHECK_UINT_EQ(out.entries[i].line, 0);
        NAOS_CHECK_FALSE(out.entries[i].obsolete);
    }

    NAOS_CHECK_FALSE(out.entries[1].has_msgctxt);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out.entries[1].msgstr.items[0]), "A");
    NAOS_CHECK_TRUE(out.entries[2].has_msgctxt);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out.entries[2].msgstr.items[0]), "MA");
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out.entries[3].msgid), "n");

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_TEST(merge_empty_old_makes_all_new)
{
    static const char template_po[] =
        "msgid \"\"\n"
        "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "#. note\n"
        "#: a.c:1\n"
        "#, c-format\n"
        "msgid \"one %d\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"file\"\n"
        "msgid_plural \"files\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n"
        "\n"
        "#~ msgid \"gone\"\n"
        "#~ msgstr \"\"\n";
    static const mnd_merge_stats_t stats = { 0, 0, 2, 0 };

    /* 既存が空なら、ヘッダを出力せず、テンプレートのメッセージをすべて新規として複製する */
    merge_test_run(template_po, NULL,
                   "#. note\n"
                   "#: a.c:1\n"
                   "#, c-format\n"
                   "msgid \"one %d\"\n"
                   "msgstr \"\"\n"
                   "\n"
                   "msgid \"file\"\n"
                   "msgid_plural \"files\"\n"
                   "msgstr[0] \"\"\n"
                   "msgstr[1] \"\"\n",
                   &stats);
}

NAOS_TEST(merge_exact_match_elements_and_previous)
{
    static const char template_po[] =
        "#. new extracted\n"
        "#: new.c:2\n"
        "#, c-format, x-custom\n"
        "msgid \"a %d\"\n"
        "msgstr \"\"\n"
        "\n"
        "#: new.c:3\n"
        "msgid \"b\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"c\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"d\"\n"
        "msgstr \"\"\n";
    static const char old_po[] =
        "# old translator\n"
        "#. old extracted\n"
        "#: old.c:1\n"
        "#, c-format, x-old\n"
        "#| msgid \"a old\"\n"
        "msgid \"a %d\"\n"
        "msgstr \"A %d\"\n"
        "\n"
        "# t2\n"
        "#, fuzzy\n"
        "#| msgid \"b old\"\n"
        "msgid \"b\"\n"
        "msgstr \"B\"\n"
        "\n"
        "#, fuzzy\n"
        "#| msgid \"c old\"\n"
        "msgid \"c\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"d\"\n"
        "msgstr \"\"\n";
    static const mnd_merge_stats_t stats = { 4, 0, 0, 0 };

    /*
     * a: 翻訳者コメントと msgstr は既存から、抽出コメント・参照・フラグ（未知のフラグを含む）は
     *    テンプレートから。fuzzy でないので前回の msgid は消す。
     * b: fuzzy の既存で訳があるので、fuzzy と既存の前回の msgid を保つ。
     * c: fuzzy の既存でも訳がなければ前回の msgid を消す。
     * d: 未翻訳の完全一致
     */
    merge_test_run(template_po, old_po,
                   "# old translator\n"
                   "#. new extracted\n"
                   "#: new.c:2\n"
                   "#, c-format, x-custom\n"
                   "msgid \"a %d\"\n"
                   "msgstr \"A %d\"\n"
                   "\n"
                   "# t2\n"
                   "#: new.c:3\n"
                   "#, fuzzy\n"
                   "#| msgid \"b old\"\n"
                   "msgid \"b\"\n"
                   "msgstr \"B\"\n"
                   "\n"
                   "#, fuzzy\n"
                   "msgid \"c\"\n"
                   "msgstr \"\"\n"
                   "\n"
                   "msgid \"d\"\n"
                   "msgstr \"\"\n",
                   &stats);
}

NAOS_TEST(merge_obsolete_revival_and_normal_precedence)
{
    static const char template_po[] =
        "msgid \"a\"\n"
        "msgstr \"\"\n"
        "\n"
        "#: b.c:1\n"
        "msgid \"b\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgctxt \"m\"\n"
        "msgid \"c\"\n"
        "msgstr \"\"\n";
    static const char old_po[] =
        "#~ msgid \"a\"\n"
        "#~ msgstr \"A old\"\n"
        "\n"
        "msgid \"a\"\n"
        "msgstr \"A\"\n"
        "\n"
        "# kept\n"
        "#~ msgid \"b\"\n"
        "#~ msgstr \"B old\"\n"
        "\n"
        "#~ msgctxt \"m\"\n"
        "#~ msgid \"c\"\n"
        "#~ msgstr \"C1\"\n"
        "\n"
        "#~ msgctxt \"m\"\n"
        "#~ msgid \"c\"\n"
        "#~ msgstr \"C2\"\n";
    static const mnd_merge_stats_t stats = { 3, 0, 0, 0 };

    /*
     * a は通常のエントリが廃止エントリより優先する。b は廃止エントリの復活（matched に数える）。
     * 同じキーの廃止エントリが複数あれば先のものを使う。使われなかった廃止エントリは
     * 既存の順で廃止エントリとして残り、通常のエントリと同じキーでも書き出しの重複にならない
     */
    merge_test_run(template_po, old_po,
                   "msgid \"a\"\n"
                   "msgstr \"A\"\n"
                   "\n"
                   "# kept\n"
                   "#: b.c:1\n"
                   "msgid \"b\"\n"
                   "msgstr \"B old\"\n"
                   "\n"
                   "msgctxt \"m\"\n"
                   "msgid \"c\"\n"
                   "msgstr \"C1\"\n"
                   "\n"
                   "#~ msgid \"a\"\n"
                   "#~ msgstr \"A old\"\n"
                   "\n"
                   "#~ msgctxt \"m\"\n"
                   "#~ msgid \"c\"\n"
                   "#~ msgstr \"C2\"\n",
                   &stats);
}

NAOS_TEST(merge_unused_entries_become_obsolete)
{
    static const char template_po[] =
        "msgid \"keep\"\n"
        "msgstr \"\"\n";
    static const char old_po[] =
        "msgid \"\"\n"
        "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "# tc\n"
        "#. ext\n"
        "#: x.c:1\n"
        "#, fuzzy, c-format\n"
        "#| msgid \"x old\"\n"
        "msgid \"x\"\n"
        "msgstr \"X\"\n"
        "\n"
        "msgid \"y\"\n"
        "msgstr \"\"\n"
        "\n"
        "#~ msgid \"z\"\n"
        "#~ msgstr \"Z\"\n"
        "\n"
        "#~ msgid \"u\"\n"
        "#~ msgstr \"\"\n"
        "\n"
        "msgctxt \"k\"\n"
        "msgid \"p\"\n"
        "msgid_plural \"ps\"\n"
        "msgstr[0] \"P0\"\n"
        "msgstr[1] \"\"\n"
        "\n"
        "msgid \"e\"\n"
        "msgid_plural \"es\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"E1\"\n";
    static const mnd_merge_stats_t stats = { 0, 0, 1, 2 };

    /*
     * 訳のあるエントリは抽出コメントと参照を消して廃止エントリにし、翻訳者コメント・フラグ・
     * 前回の msgid・msgctxt・msgid_plural・msgstr を保つ（手順 6 の整理は行わない）。
     * 訳のない y、u、最初の msgstr が空の e は出力しない。元からの廃止エントリ z は数えない
     */
    merge_test_run(template_po, old_po,
                   "msgid \"\"\n"
                   "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                   "\n"
                   "msgid \"keep\"\n"
                   "msgstr \"\"\n"
                   "\n"
                   "# tc\n"
                   "#, fuzzy, c-format\n"
                   "#~| msgid \"x old\"\n"
                   "#~ msgid \"x\"\n"
                   "#~ msgstr \"X\"\n"
                   "\n"
                   "#~ msgid \"z\"\n"
                   "#~ msgstr \"Z\"\n"
                   "\n"
                   "#~ msgctxt \"k\"\n"
                   "#~ msgid \"p\"\n"
                   "#~ msgid_plural \"ps\"\n"
                   "#~ msgstr[0] \"P0\"\n"
                   "#~ msgstr[1] \"\"\n",
                   &stats);
}

NAOS_TEST(merge_obsolete_keeps_previous_of_untranslated_fuzzy)
{
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_diag_list_t diags;
    mnd_merge_stats_t stats;
    mnd_po_entry_t *entry;

    mnd_po_catalog_init(&template_catalog);
    mnd_po_catalog_init(&old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);

    /*
     * 廃止エントリに手順 6 を行わないことを確かめる: 訳があり fuzzy でない既存のエントリの
     * 前回の msgid は、廃止エントリでもそのまま残る
     */
    merge_test_add(&old_catalog, NULL, "x", "X", false, 1);
    entry = &old_catalog.entries[0];
    entry->has_prev_msgid = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid, "x old"));

    NAOS_REQUIRE_TRUE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                                         &out, &stats, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    NAOS_REQUIRE_UINT_EQ(out.count, 1);
    NAOS_CHECK_TRUE(out.entries[0].obsolete);
    NAOS_CHECK_TRUE(out.entries[0].has_prev_msgid);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out.entries[0].prev_msgid), "x old");
    NAOS_CHECK_UINT_EQ(stats.obsoleted, 1);
    NAOS_CHECK_UINT_EQ(stats.matched + stats.fuzzy + stats.added, 0);

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_TEST(merge_plural_cases_use_nplurals_of_old_header)
{
    static const char template_po[] =
        "msgid \"ss\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"s2p\"\n"
        "msgid_plural \"s2p many\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n"
        "\n"
        "msgid \"p2s\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"pp\"\n"
        "msgid_plural \"pp many\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n"
        "\n"
        "msgid \"pd\"\n"
        "msgid_plural \"pd new\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n"
        "\n"
        "msgid \"new\"\n"
        "msgid_plural \"news\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n";
    static const char old_po[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"Plural-Forms: nplurals=3; plural=n%10==1 ? 0 : 1;\\n\"\n"
        "\n"
        "msgid \"ss\"\n"
        "msgstr \"T\"\n"
        "\n"
        "msgid \"s2p\"\n"
        "msgstr \"S\"\n"
        "\n"
        "msgid \"p2s\"\n"
        "msgid_plural \"p2s many\"\n"
        "msgstr[0] \"P0\"\n"
        "msgstr[1] \"P1\"\n"
        "msgstr[2] \"P2\"\n"
        "\n"
        "msgid \"pp\"\n"
        "msgid_plural \"pp many\"\n"
        "msgstr[0] \"Q0\"\n"
        "msgstr[1] \"Q1\"\n"
        "msgstr[2] \"Q2\"\n"
        "\n"
        "msgid \"pd\"\n"
        "msgid_plural \"pd old\"\n"
        "msgstr[0] \"R0\"\n"
        "msgstr[1] \"R1\"\n"
        "msgstr[2] \"R2\"\n";
    static const mnd_merge_stats_t stats = { 5, 0, 1, 0 };

    /*
     * 両方が単数形、単数形 → 複数形（nplurals 個に複製して fuzzy）、複数形 → 単数形（最初の
     * msgstr だけにして fuzzy）、両方が複数形（そのまま。msgid_plural が異なれば fuzzy）、
     * 新規の複数形（空文字列 nplurals 個）。複数形の食い違いで fuzzy にしたものも matched に数える
     */
    merge_test_run(template_po, old_po,
                   "msgid \"\"\n"
                   "msgstr \"\"\n"
                   "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                   "\"Plural-Forms: nplurals=3; plural=n%10==1 ? 0 : 1;\\n\"\n"
                   "\n"
                   "msgid \"ss\"\n"
                   "msgstr \"T\"\n"
                   "\n"
                   "#, fuzzy\n"
                   "#| msgid \"s2p\"\n"
                   "msgid \"s2p\"\n"
                   "msgid_plural \"s2p many\"\n"
                   "msgstr[0] \"S\"\n"
                   "msgstr[1] \"S\"\n"
                   "msgstr[2] \"S\"\n"
                   "\n"
                   "#, fuzzy\n"
                   "#| msgid \"p2s\"\n"
                   "#| msgid_plural \"p2s many\"\n"
                   "msgid \"p2s\"\n"
                   "msgstr \"P0\"\n"
                   "\n"
                   "msgid \"pp\"\n"
                   "msgid_plural \"pp many\"\n"
                   "msgstr[0] \"Q0\"\n"
                   "msgstr[1] \"Q1\"\n"
                   "msgstr[2] \"Q2\"\n"
                   "\n"
                   "#, fuzzy\n"
                   "#| msgid \"pd\"\n"
                   "#| msgid_plural \"pd old\"\n"
                   "msgid \"pd\"\n"
                   "msgid_plural \"pd new\"\n"
                   "msgstr[0] \"R0\"\n"
                   "msgstr[1] \"R1\"\n"
                   "msgstr[2] \"R2\"\n"
                   "\n"
                   "msgid \"new\"\n"
                   "msgid_plural \"news\"\n"
                   "msgstr[0] \"\"\n"
                   "msgstr[1] \"\"\n"
                   "msgstr[2] \"\"\n",
                   &stats);
}

NAOS_TEST(merge_fuzzy_match_elements_and_previous)
{
    static const char template_po[] =
        "msgid \"\"\n"
        "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "#. new extracted\n"
        "#: new.c:1\n"
        "#, c-format\n"
        "msgctxt \"menu\"\n"
        "msgid \"Open the file %s\"\n"
        "msgstr \"\"\n"
        "\n"
        "#: new.c:2\n"
        "msgid \"%d file removed\"\n"
        "msgid_plural \"%d files removed\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n"
        "\n"
        "msgid \"Save all documents now\"\n"
        "msgstr \"\"\n";
    static const char old_po[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"Plural-Forms: nplurals=2; plural=n != 1;\\n\"\n"
        "\n"
        "# tc\n"
        "#. old extracted\n"
        "#: old.c:1\n"
        "#, c-format\n"
        "msgctxt \"dialog\"\n"
        "msgid \"Open a file %s\"\n"
        "msgstr \"Ouvrir %s\"\n"
        "\n"
        "msgid \"%d file deleted\"\n"
        "msgid_plural \"%d files deleted\"\n"
        "msgstr[0] \"D0\"\n"
        "msgstr[1] \"D1\"\n"
        "\n"
        "# t3\n"
        "#, fuzzy\n"
        "#| msgctxt \"old\"\n"
        "#| msgid \"Save all docs\"\n"
        "msgid \"Save all documents\"\n"
        "msgstr \"Tout enregistrer\"\n";
    static const mnd_merge_options_t options = MND_MERGE_OPTIONS_INIT;
    static const mnd_merge_stats_t stats = { 0, 3, 0, 0 };

    /*
     * あいまい一致では、msgstr と翻訳者コメントを候補から、抽出コメント・参照・フラグを
     * テンプレートから取り、fuzzy を付ける。前回の msgid は、fuzzy でない候補なら候補の
     * msgctxt・msgid・msgid_plural（msgctxt が異なる候補も使う）、fuzzy の候補なら候補の前回の
     * msgid。使われた候補は廃止エントリにならない（msgmerge --previous 0.21 の出力と同じ）
     */
    merge_test_run_options(&options, template_po, old_po,
                           "msgid \"\"\n"
                           "msgstr \"\"\n"
                           "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                           "\"Plural-Forms: nplurals=2; plural=n != 1;\\n\"\n"
                           "\n"
                           "# tc\n"
                           "#. new extracted\n"
                           "#: new.c:1\n"
                           "#, fuzzy, c-format\n"
                           "#| msgctxt \"dialog\"\n"
                           "#| msgid \"Open a file %s\"\n"
                           "msgctxt \"menu\"\n"
                           "msgid \"Open the file %s\"\n"
                           "msgstr \"Ouvrir %s\"\n"
                           "\n"
                           "#: new.c:2\n"
                           "#, fuzzy\n"
                           "#| msgid \"%d file deleted\"\n"
                           "#| msgid_plural \"%d files deleted\"\n"
                           "msgid \"%d file removed\"\n"
                           "msgid_plural \"%d files removed\"\n"
                           "msgstr[0] \"D0\"\n"
                           "msgstr[1] \"D1\"\n"
                           "\n"
                           "# t3\n"
                           "#, fuzzy\n"
                           "#| msgctxt \"old\"\n"
                           "#| msgid \"Save all docs\"\n"
                           "msgid \"Save all documents now\"\n"
                           "msgstr \"Tout enregistrer\"\n",
                           &stats);
}

/* あいまい一致の有効・無効を比べる入力（テンプレート） */
static const char merge_test_shared_template_po[] =
    "msgid \"Print the document.\"\n"
    "msgstr \"\"\n"
    "\n"
    "msgid \"Zebra\"\n"
    "msgstr \"\"\n"
    "\n"
    "msgid \"Print the documents\"\n"
    "msgstr \"\"\n";

/* あいまい一致の有効・無効を比べる入力（既存。候補は廃止エントリ） */
static const char merge_test_shared_old_po[] =
    "msgid \"\"\n"
    "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
    "\n"
    "msgid \"Quit\"\n"
    "msgstr \"Quitter\"\n"
    "\n"
    "#~ msgid \"Print the document\"\n"
    "#~ msgstr \"Imprimer le document\"\n";

NAOS_TEST(merge_fuzzy_candidate_shared_and_obsolete_used)
{
    static const mnd_merge_stats_t stats = { 0, 2, 1, 1 };

    /*
     * options が NULL なら既定であいまい一致を行う。1 つの廃止エントリを 2 つのテンプレートの
     * エントリが候補として使い、使われた廃止エントリは廃止エントリとして出力しない。
     * 候補のない Zebra は新規、使われなかった Quit は廃止エントリになる
     */
    merge_test_run(merge_test_shared_template_po, merge_test_shared_old_po,
                   "msgid \"\"\n"
                   "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                   "\n"
                   "#, fuzzy\n"
                   "#| msgid \"Print the document\"\n"
                   "msgid \"Print the document.\"\n"
                   "msgstr \"Imprimer le document\"\n"
                   "\n"
                   "msgid \"Zebra\"\n"
                   "msgstr \"\"\n"
                   "\n"
                   "#, fuzzy\n"
                   "#| msgid \"Print the document\"\n"
                   "msgid \"Print the documents\"\n"
                   "msgstr \"Imprimer le document\"\n"
                   "\n"
                   "#~ msgid \"Quit\"\n"
                   "#~ msgstr \"Quitter\"\n",
                   &stats);
}

NAOS_TEST(merge_fuzzy_disabled_makes_unmatched_new)
{
    static const mnd_merge_options_t options = { false };
    static const mnd_merge_stats_t stats = { 0, 0, 3, 1 };

    /*
     * あいまい一致が無効なら、完全一致しないエントリはすべて新規になり、候補になり得た
     * 廃止エントリも廃止エントリのまま残る（msgmerge --no-fuzzy-matching と同じ）
     */
    merge_test_run_options(&options, merge_test_shared_template_po, merge_test_shared_old_po,
                           "msgid \"\"\n"
                           "msgstr \"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                           "\n"
                           "msgid \"Print the document.\"\n"
                           "msgstr \"\"\n"
                           "\n"
                           "msgid \"Zebra\"\n"
                           "msgstr \"\"\n"
                           "\n"
                           "msgid \"Print the documents\"\n"
                           "msgstr \"\"\n"
                           "\n"
                           "#~ msgid \"Quit\"\n"
                           "#~ msgstr \"Quitter\"\n"
                           "\n"
                           "#~ msgid \"Print the document\"\n"
                           "#~ msgstr \"Imprimer le document\"\n",
                           &stats);
}

NAOS_TEST(merge_stats_with_null_options_and_null_stats)
{
    static const char template_po[] =
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

    /* stats が NULL でもマージできる */
    NAOS_REQUIRE_TRUE(mnd_merge_catalogs(&template_catalog, "t.pot", &old_catalog, "o.po", NULL,
                                         &out, NULL, &diags));
    NAOS_CHECK_UINT_EQ(mnd_diag_list_count(&diags), 0);
    TEST_CHECK_WRITTEN(&out, old_po);

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
    NAOS_ADD_TEST(merge_output_order_and_template_obsolete_ignored),
    NAOS_ADD_TEST(merge_output_is_one_to_one_with_template),
    NAOS_ADD_TEST(merge_empty_old_makes_all_new),
    NAOS_ADD_TEST(merge_exact_match_elements_and_previous),
    NAOS_ADD_TEST(merge_obsolete_revival_and_normal_precedence),
    NAOS_ADD_TEST(merge_unused_entries_become_obsolete),
    NAOS_ADD_TEST(merge_obsolete_keeps_previous_of_untranslated_fuzzy),
    NAOS_ADD_TEST(merge_plural_cases_use_nplurals_of_old_header),
    NAOS_ADD_TEST(merge_fuzzy_match_elements_and_previous),
    NAOS_ADD_TEST(merge_fuzzy_candidate_shared_and_obsolete_used),
    NAOS_ADD_TEST(merge_fuzzy_disabled_makes_unmatched_new),
    NAOS_ADD_TEST(merge_stats_with_null_options_and_null_stats),
    NAOS_ADD_TEST(merge_scaffold_test_util_round_trips_po)
)
