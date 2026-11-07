/* スイート translate: 再構成（Rebuilder）（4.1〜4.7、5.1、5.2、6.1〜6.4、7.4、7.7） */

#include <naos/naos.h>

#include "test_util.h"
#include "tu/tu.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"
#include "po/po.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 統計の 5 つの件数が期待どおりであることを検査する */
static void check_stats(const mnd_tu_stats_t *stats, size_t total, size_t translated, size_t fuzzy,
                        size_t untranslated, size_t missing)
{
    NAOS_CHECK_UINT_EQ(total, stats->total);
    NAOS_CHECK_UINT_EQ(translated, stats->translated);
    NAOS_CHECK_UINT_EQ(fuzzy, stats->fuzzy);
    NAOS_CHECK_UINT_EQ(untranslated, stats->untranslated);
    NAOS_CHECK_UINT_EQ(missing, stats->missing);
}

#define CHECK_STATS(stats, total, translated, fuzzy, untranslated, missing) \
    do \
    { \
        NAOS_TRACE_PUSH("in CHECK_STATS"); \
        check_stats((stats), (total), (translated), (fuzzy), (untranslated), (missing)); \
        NAOS_TRACE_POP(); \
    } while (0)

/*
 * catalog の末尾に msgid と msgstr[0..msgstr_len) のエントリを直接加える（mnd_po_read を通さない）。
 * line はエントリの行番号。fuzzy なら fuzzy のフラグを付ける
 */
static void add_raw_entry(mnd_po_catalog_t *catalog, const char *msgid, const char *msgstr,
                          size_t msgstr_len, size_t line, bool fuzzy)
{
    mnd_po_entry_t *entry = mnd_po_catalog_append(catalog);

    NAOS_REQUIRE_NOT_NULL(entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->msgid, msgid));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry->msgstr, msgstr, msgstr_len));
    if (fuzzy)
    {
        NAOS_REQUIRE_TRUE(mnd_po_flag_add(entry, "fuzzy"));
    }
    entry->line = line;
}

/*
 * text（キーパス keys[0..key_count)）を走査し、抽出したすべての msgid に msgstr = msgid を与えた
 * カタログで再構成した出力が原文と同一であることを検査する
 */
static void check_identity_catalog(const char *text, const char *const *keys, size_t key_count)
{
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;
    size_t i;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, keys, key_count, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_REQUIRE_TRUE(mnd_tu_extract(doc, "test.md", &catalog, &diags));
    NAOS_REQUIRE_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_UINT_GT(catalog.count, 0);
    for (i = 0; i < catalog.count; i++)
    {
        mnd_po_entry_t *entry = &catalog.entries[i];

        if (entry->msgstr.count == 0)
        {
            NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry->msgstr, entry->msgid.data, entry->msgid.len));
        }
        else
        {
            mnd_buf_clear(&entry->msgstr.items[0]);
            NAOS_REQUIRE_TRUE(mnd_buf_append(&entry->msgstr.items[0], entry->msgid.data,
                                             entry->msgid.len));
        }
    }
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, text, strlen(text), &stats);
    CHECK_STATS(&stats, mnd_tu_doc_segment_count(doc), mnd_tu_doc_segment_count(doc), 0, 0, 0);
    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/*
 * text の再構成が true を返し、出力が expected で、診断が warning の 1 つだけ
 * （メッセージ message、path "test.md"、行 line）であることを検査する
 */
static void check_one_warning(const char *text, const char *po, const char *expected,
                              const char *message, size_t line)
{
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    const mnd_diag_t *diag;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO(po, &catalog);
    NAOS_CHECK_TRUE(mnd_tu_translate(doc, &catalog, NULL, &out, NULL, &diags));
    NAOS_CHECK_FALSE(mnd_diag_list_has_error(&diags));
    TEST_CHECK_BYTES(out.data, out.len, expected, strlen(expected));
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ((int)MND_SEVERITY_WARNING, (int)diag->severity);
    NAOS_CHECK_STR_EQ(message, diag->message);
    NAOS_REQUIRE_NOT_NULL(diag->path);
    NAOS_CHECK_STR_EQ("test.md", diag->path);
    NAOS_CHECK_UINT_EQ(line, diag->line);
    mnd_diag_list_free(&diags);
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

#define CHECK_ONE_WARNING(text, po, expected, message, line) \
    do \
    { \
        NAOS_TRACE_PUSH("in CHECK_ONE_WARNING"); \
        check_one_warning((text), (po), (expected), (message), (line)); \
        NAOS_TRACE_POP(); \
    } while (0)

/*
 * 再構成が false を返し、out が空、*stats がすべて 0 で、診断が error の 1 つだけ
 * （メッセージ message、path "ja.po"、行 line）であることを検査する。out と stats は事前に埋めておく
 */
static void check_translation_error(const mnd_tu_doc_t *doc, const mnd_po_catalog_t *catalog,
                                    bool use_fuzzy, const char *message, size_t line)
{
    mnd_tu_translate_options_t options = MND_TU_TRANSLATE_OPTIONS_INIT;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_stats_t stats = { 9, 9, 9, 9, 9 };
    const mnd_diag_t *diag;

    options.use_fuzzy = use_fuzzy;
    options.catalog_path = "ja.po";
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "stale"));
    NAOS_CHECK_FALSE(mnd_tu_translate(doc, catalog, &options, &out, &stats, &diags));
    NAOS_CHECK_UINT_EQ(0, out.len);
    CHECK_STATS(&stats, 0, 0, 0, 0, 0);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)diag->severity);
    NAOS_CHECK_STR_EQ(message, diag->message);
    NAOS_REQUIRE_NOT_NULL(diag->path);
    NAOS_CHECK_STR_EQ("ja.po", diag->path);
    NAOS_CHECK_UINT_EQ(line, diag->line);
    mnd_diag_list_free(&diags);
    mnd_buf_free(&out);
}

#define CHECK_TRANSLATION_ERROR(doc, catalog, use_fuzzy, message, line) \
    do \
    { \
        NAOS_TRACE_PUSH("in CHECK_TRANSLATION_ERROR"); \
        check_translation_error((doc), (catalog), (use_fuzzy), (message), (line)); \
        NAOS_TRACE_POP(); \
    } while (0)

/* 再構成のオプションの既定値は fuzzy 不採用・LF・パスなしで、統計の既定値はすべて 0 */
NAOS_TEST(translate_options_and_stats_init)
{
    mnd_tu_translate_options_t options = MND_TU_TRANSLATE_OPTIONS_INIT;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    NAOS_CHECK_FALSE(options.use_fuzzy);
    NAOS_CHECK_INT_EQ((int)MND_EOL_LF, (int)options.eol);
    NAOS_CHECK_TRUE(options.catalog_path == NULL);
    NAOS_CHECK_UINT_EQ(0, stats.total);
    NAOS_CHECK_UINT_EQ(0, stats.translated);
    NAOS_CHECK_UINT_EQ(0, stats.fuzzy);
    NAOS_CHECK_UINT_EQ(0, stats.untranslated);
    NAOS_CHECK_UINT_EQ(0, stats.missing);
}

/* 翻訳済みのエントリは msgstr で置き換え、カタログにないセグメントは原文のまま（4.4、4.5、6.1） */
NAOS_TEST(translate_translated_and_missing)
{
    static const char text[] =
        "Hello\n"
        "\n"
        "World\n";
    static const char expected[] =
        "Bonjour\n"
        "\n"
        "World\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("msgid \"Hello\"\nmsgstr \"Bonjour\"\n", &catalog);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, expected, strlen(expected), &stats);
    CHECK_STATS(&stats, 2, 1, 0, 0, 1);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* msgstr が空のエントリは原文のまま出し、未翻訳として数える（4.5、6.1） */
NAOS_TEST(translate_untranslated_keeps_source)
{
    static const char text[] = "# Title\n\nBody\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("msgid \"Title\"\nmsgstr \"\"\n\nmsgid \"Body\"\nmsgstr \"\"\n", &catalog);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, text, strlen(text), &stats);
    CHECK_STATS(&stats, 2, 0, 0, 2, 0);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* カタログに一致がなければ原文のまま出し、該当なしとして数える（4.5、6.1） */
NAOS_TEST(translate_missing_keeps_source)
{
    static const char text[] = "Alpha\n\n- Beta\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("msgid \"Gamma\"\nmsgstr \"Gamme\"\n", &catalog);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, text, strlen(text), &stats);
    CHECK_STATS(&stats, 2, 0, 0, 0, 2);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* fuzzy は既定では採用せず原文を出し、fuzzy として数える（4.6、6.3） */
NAOS_TEST(translate_fuzzy_not_adopted_by_default)
{
    static const char text[] = "Hello\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("#, fuzzy\nmsgid \"Hello\"\nmsgstr \"Bonjour\"\n", &catalog);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, text, strlen(text), &stats);
    CHECK_STATS(&stats, 1, 0, 1, 0, 0);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* use_fuzzy なら fuzzy の msgstr を採用し、翻訳済みではなく fuzzy として数える（4.7、6.3） */
NAOS_TEST(translate_fuzzy_adopted_with_use_fuzzy)
{
    static const char text[] = "Hello\n";
    mnd_tu_translate_options_t options = MND_TU_TRANSLATE_OPTIONS_INIT;
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    options.use_fuzzy = true;
    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("#, fuzzy\nmsgid \"Hello\"\nmsgstr \"Bonjour\"\n", &catalog);
    TEST_CHECK_TRANSLATED(doc, &catalog, &options, "Bonjour\n", 8, &stats);
    CHECK_STATS(&stats, 1, 0, 1, 0, 0);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* msgctxt 付きのエントリと廃止エントリは検索の対象外で、該当なしとして数える（4.3） */
NAOS_TEST(translate_ignores_msgctxt_and_obsolete_entries)
{
    static const char text[] = "Hello\n";
    static const char po[] =
        "msgctxt \"menu\"\n"
        "msgid \"Hello\"\n"
        "msgstr \"Menu\"\n"
        "\n"
        "#~ msgid \"Hello\"\n"
        "#~ msgstr \"Old\"\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO(po, &catalog);
    NAOS_REQUIRE_UINT_EQ(2, catalog.count);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, text, strlen(text), &stats);
    CHECK_STATS(&stats, 1, 0, 0, 0, 1);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 同じ msgid の 3 回の出現はそれぞれ置き換え、出現ごとに数える（6.1、6.2） */
NAOS_TEST(translate_counts_each_occurrence)
{
    static const char text[] =
        "# Same\n"
        "\n"
        "Same\n"
        "\n"
        "> Same\n"
        "\n"
        "Other\n";
    static const char expected[] =
        "# Pareil\n"
        "\n"
        "Pareil\n"
        "\n"
        "> Pareil\n"
        "\n"
        "Other\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("msgid \"Same\"\nmsgstr \"Pareil\"\n\nmsgid \"Other\"\nmsgstr \"\"\n", &catalog);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, expected, strlen(expected), &stats);
    CHECK_STATS(&stats, 4, 3, 0, 1, 0);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 本文とフロントマターの両方を置き換え、セグメント以外のバイトは原文のまま（4.1、4.2、4.10） */
NAOS_TEST(translate_replaces_body_and_front_matter)
{
    static const char *const keys[] = { "title", "summary" };
    static const char text[] =
        "---\n"
        "title: Hello\n"
        "summary: \"Body\"\n"
        "date: 2024-01-01\n"
        "---\n"
        "# Hello\n"
        "\n"
        "```\n"
        "Hello\n"
        "```\n"
        "\n"
        "* Body\n";
    static const char expected[] =
        "---\n"
        "title: Bonjour\n"
        "summary: \"Corps\"\n"
        "date: 2024-01-01\n"
        "---\n"
        "# Bonjour\n"
        "\n"
        "```\n"
        "Hello\n"
        "```\n"
        "\n"
        "* Corps\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    TEST_SCAN(text, keys, 2, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("msgid \"Hello\"\nmsgstr \"Bonjour\"\n\nmsgid \"Body\"\nmsgstr \"Corps\"\n", &catalog);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, expected, strlen(expected), &stats);
    CHECK_STATS(&stats, 4, 4, 0, 0, 0);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* フロントマターの訳文は構造の検査をせず、記号で始まっても警告しない（4.11） */
NAOS_TEST(translate_does_not_check_front_matter)
{
    static const char *const keys[] = { "title" };
    static const char text[] =
        "---\n"
        "title: \"Hello\"\n"
        "---\n";
    static const char expected[] =
        "---\n"
        "title: \"# Bonjour\"\n"
        "---\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;

    TEST_SCAN(text, keys, 1, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("msgid \"Hello\"\nmsgstr \"# Bonjour\"\n", &catalog);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, expected, strlen(expected), NULL);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 原文より多い訳文の行の境界には options の eol を使う（4.9） */
NAOS_TEST(translate_uses_eol_for_extra_lines)
{
    static const char text[] = "> Hello\r\n";
    static const char expected[] = "> Bon\r\n> jour\r\n";
    mnd_tu_translate_options_t options = MND_TU_TRANSLATE_OPTIONS_INIT;
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;

    options.eol = MND_EOL_CRLF;
    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("msgid \"Hello\"\nmsgstr \"Bon\\njour\"\n", &catalog);
    TEST_CHECK_TRANSLATED(doc, &catalog, &options, expected, strlen(expected), NULL);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 訳文の行がブロックの記号で始まれば警告を 1 つ出し、true と訳文どおりの出力を返す（4.11、7.7） */
NAOS_TEST(translate_warns_block_marker_and_succeeds)
{
    CHECK_ONE_WARNING("Intro\n\nPara\n",
                      "msgid \"Para\"\nmsgstr \"first\\n> quote\"\n",
                      "Intro\n\nfirst\n> quote\n",
                      "translation line 2 starts with a block marker (msgid at line 3)", 3);
}

/* 訳文に空行があれば警告を 1 つ出し、訳文どおりに出力する（4.11、7.7） */
NAOS_TEST(translate_warns_blank_line_and_succeeds)
{
    CHECK_ONE_WARNING("Intro\n\nPara\n",
                      "msgid \"Para\"\nmsgstr \"a\\n\\nb\"\n",
                      "Intro\n\na\n\nb\n",
                      "translation contains a blank line (msgid at line 3)", 3);
}

/* ATX 見出しの訳文に改行があれば警告を 1 つ出し、訳文どおりに出力する（4.11、7.7） */
NAOS_TEST(translate_warns_heading_line_break_and_succeeds)
{
    CHECK_ONE_WARNING("Intro\n\n# Head\n",
                      "msgid \"Head\"\nmsgstr \"a\\nb\"\n",
                      "Intro\n\n# a\nb\n",
                      "translation of a one-line heading contains a line break (msgid at line 3)", 3);
}

/* 採用した訳文の不正な UTF-8 はエラーで、catalog_path とエントリの行を添え、出力と統計を空にする（7.4） */
NAOS_TEST(translate_invalid_utf8_translation_is_error)
{
    static const char text[] = "Before\n\nHello\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    add_raw_entry(&catalog, "Before", "Avant", 5, 3, false);
    add_raw_entry(&catalog, "Hello", "Bon\xffjour", 8, 12, false);
    CHECK_TRANSLATION_ERROR(doc, &catalog, false, "invalid UTF-8 sequence in translation", 12);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 採用した訳文の NUL はエラーで、catalog_path とエントリの行を添え、出力と統計を空にする（7.4） */
NAOS_TEST(translate_nul_translation_is_error)
{
    static const char text[] = "Hello\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    add_raw_entry(&catalog, "Hello", "Bon\0jour", 8, 21, true);
    CHECK_TRANSLATION_ERROR(doc, &catalog, true, "NUL in translation", 21);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 採用しない fuzzy の訳文は検査しない（7.4 は採用した訳文だけが対象） */
NAOS_TEST(translate_does_not_check_unadopted_fuzzy)
{
    static const char text[] = "Hello\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    add_raw_entry(&catalog, "Hello", "Bon\xffjour", 8, 4, true);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, text, strlen(text), &stats);
    CHECK_STATS(&stats, 1, 0, 1, 0, 0);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* 空のカタログでは出力が原文とバイト単位で同一（5.1） */
NAOS_TEST(translate_empty_catalog_is_identity)
{
    static const char *const keys[] = { "title" };
    static const char text[] =
        "---\n"
        "title: >\n"
        "  Folded\n"
        "  title\n"
        "---\n"
        "# Heading  \r\n"
        "\r\n"
        "> quoted\n"
        "lazy line\n"
        "\n"
        "1. item\n"
        "   continued\n"
        "\n"
        "    code\n"
        "tail without newline";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    mnd_po_catalog_init(&catalog);
    TEST_SCAN(text, keys, 1, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, text, strlen(text), &stats);
    CHECK_STATS(&stats, mnd_tu_doc_segment_count(doc), 0, 0, 0, mnd_tu_doc_segment_count(doc));
    NAOS_CHECK_UINT_EQ(5, stats.total);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* msgstr == msgid のカタログでは、折り返しスカラーのフロントマターを含めて原文と同一（5.2） */
NAOS_TEST(translate_identity_catalog_is_identity)
{
    static const char *const keys[] = { "title", "description" };
    static const char text[] =
        "---\n"
        "title: Hello\n"
        "description: >\n"
        "  Folded\n"
        "  scalar\n"
        "\n"
        "  text\n"
        "---\n"
        "# Hello  \n"
        "\n"
        "> quoted line\n"
        "lazy continuation  \n"
        "> third\\\n"
        "\n"
        "- item\n"
        "\tcontinued\r\n"
        "\n"
        "Setext\n"
        "===\n";

    check_identity_catalog(text, keys, 2);
}

/* options と stats は NULL でよい（MND_TU_TRANSLATE_OPTIONS_INIT と同じ） */
NAOS_TEST(translate_accepts_null_options_and_stats)
{
    static const char text[] = "Hello\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("#, fuzzy\nmsgid \"Hello\"\nmsgstr \"Bonjour\"\n", &catalog);
    TEST_CHECK_TRANSLATED(doc, &catalog, NULL, text, strlen(text), NULL);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

/* out の既存の内容は捨て、出力だけを入れる */
NAOS_TEST(translate_discards_previous_out_contents)
{
    static const char text[] = "Hello\n";
    mnd_tu_doc_t *doc = NULL;
    mnd_po_catalog_t catalog;
    mnd_buf_t out = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    TEST_SCAN(text, NULL, 0, doc);
    NAOS_REQUIRE_NOT_NULL(doc);
    TEST_READ_PO("msgid \"Hello\"\nmsgstr \"Bonjour\"\n", &catalog);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "previous contents"));
    NAOS_CHECK_TRUE(mnd_tu_translate(doc, &catalog, NULL, &out, NULL, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    TEST_CHECK_BYTES(out.data, out.len, "Bonjour\n", 8);
    mnd_diag_list_free(&diags);
    mnd_buf_free(&out);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
}

NAOS_SUITE(translate,
    NAOS_ADD_TEST(translate_options_and_stats_init),
    NAOS_ADD_TEST(translate_translated_and_missing),
    NAOS_ADD_TEST(translate_untranslated_keeps_source),
    NAOS_ADD_TEST(translate_missing_keeps_source),
    NAOS_ADD_TEST(translate_fuzzy_not_adopted_by_default),
    NAOS_ADD_TEST(translate_fuzzy_adopted_with_use_fuzzy),
    NAOS_ADD_TEST(translate_ignores_msgctxt_and_obsolete_entries),
    NAOS_ADD_TEST(translate_counts_each_occurrence),
    NAOS_ADD_TEST(translate_replaces_body_and_front_matter),
    NAOS_ADD_TEST(translate_does_not_check_front_matter),
    NAOS_ADD_TEST(translate_uses_eol_for_extra_lines),
    NAOS_ADD_TEST(translate_warns_block_marker_and_succeeds),
    NAOS_ADD_TEST(translate_warns_blank_line_and_succeeds),
    NAOS_ADD_TEST(translate_warns_heading_line_break_and_succeeds),
    NAOS_ADD_TEST(translate_invalid_utf8_translation_is_error),
    NAOS_ADD_TEST(translate_nul_translation_is_error),
    NAOS_ADD_TEST(translate_does_not_check_unadopted_fuzzy),
    NAOS_ADD_TEST(translate_empty_catalog_is_identity),
    NAOS_ADD_TEST(translate_identity_catalog_is_identity),
    NAOS_ADD_TEST(translate_accepts_null_options_and_stats),
    NAOS_ADD_TEST(translate_discards_previous_out_contents)
)
