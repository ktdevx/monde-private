/* Builder: エントリの複製と、完全一致・あいまい一致・新規・廃止エントリの組み立て */

#include <naos/naos.h>

#include "po_merge/merge.h"
#include "po_merge/merge_internal.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* list が expected の count 個の文字列と順に等しいことを調べる */
static void check_strings(const mnd_po_strings_t *list, const char *const *expected, size_t count)
{
    size_t i;

    NAOS_REQUIRE_UINT_EQ(count, list->count);
    for (i = 0; i < count; i++)
    {
        NAOS_CHECK_STR_EQ(expected[i], mnd_buf_cstr(&list->items[i]));
    }
}

#define CHECK_STRINGS(list, expected, count) \
    do \
    { \
        NAOS_TRACE_PUSH("in CHECK_STRINGS"); \
        check_strings((list), (expected), (count)); \
        NAOS_TRACE_POP(); \
    } while (0)

/* catalog の各エントリの文字列の内容を書き換え、カタログを解放する（入力の領域を壊す） */
static void destroy_catalog(mnd_po_catalog_t *catalog)
{
    size_t i;
    size_t j;

    for (i = 0; i < catalog->count; i++)
    {
        mnd_po_entry_t *entry = &catalog->entries[i];

        if (entry->msgid.len > 0)
        {
            memset(entry->msgid.data, 'X', entry->msgid.len);
        }

        for (j = 0; j < entry->msgstr.count; j++)
        {
            if (entry->msgstr.items[j].len > 0)
            {
                memset(entry->msgstr.items[j].data, 'X', entry->msgstr.items[j].len);
            }
        }
    }

    mnd_po_catalog_free(catalog);
}

/*
 * old_text と template_text の最初のエントリから、nplurals を渡して combine で
 * result（初期化済み）を作る
 */
static void combine_texts(const char *old_text, const char *template_text, bool force_fuzzy,
                          size_t nplurals, mnd_po_entry_t *result)
{
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;

    TEST_READ_PO(old_text, &old_catalog);
    TEST_READ_PO(template_text, &template_catalog);
    NAOS_REQUIRE_UINT_EQ(1, old_catalog.count);
    NAOS_REQUIRE_UINT_EQ(1, template_catalog.count);

    NAOS_REQUIRE_TRUE(mnd_merge_combine(result, &old_catalog.entries[0],
                                        &template_catalog.entries[0], force_fuzzy,
                                        nplurals));

    /* 結果は入力と領域を共有しないので、入力を壊して解放しても結果は変わらない */
    destroy_catalog(&template_catalog);
    destroy_catalog(&old_catalog);
}

#define COMBINE_TEXTS_N(old_text, template_text, force_fuzzy, nplurals, result) \
    do \
    { \
        NAOS_TRACE_PUSH("in COMBINE_TEXTS_N"); \
        combine_texts((old_text), (template_text), (force_fuzzy), (nplurals), (result)); \
        NAOS_TRACE_POP(); \
    } while (0)

/* nplurals を 2（Plural-Forms のないときの値）として combine する */
#define COMBINE_TEXTS(old_text, template_text, force_fuzzy, result) \
    COMBINE_TEXTS_N((old_text), (template_text), (force_fuzzy), 2, (result))

NAOS_TEST(entry_scaffold_default_options_enable_fuzzy_matching)
{
    mnd_merge_options_t options = MND_MERGE_OPTIONS_INIT;

    /* 既定はあいまい一致あり（msgmerge の既定と同じ） */
    NAOS_CHECK_TRUE(options.fuzzy_matching);
}

NAOS_TEST(entry_combine_takes_elements_by_table)
{
    static const char *const translator[] = { "old translator" };
    static const char *const extracted[] = { "template note" };
    static const char *const flags[] = { "fuzzy", "no-c-format", "x-unknown" };
    static const char *const msgstr[] = { "こんにちは、世界" };
    mnd_po_entry_t result;

    /* あいまい一致: 名前の要素は r、訳と翻訳者コメントは d、抽出コメントと参照は r から */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "# old translator\n"
        "#. old note\n"
        "#: old.c:1\n"
        "#, c-format\n"
        "msgctxt \"menu\"\n"
        "msgid \"Hello world\"\n"
        "msgstr \"こんにちは、世界\"\n",
        "# template translator\n"
        "#. template note\n"
        "#: new.c:10 other.c\n"
        "#, no-c-format, x-unknown\n"
        "msgctxt \"menu2\"\n"
        "msgid \"Hello, world\"\n"
        "msgstr \"\"\n",
        true, &result);

    NAOS_CHECK_TRUE(result.has_msgctxt);
    NAOS_CHECK_STR_EQ("menu2", mnd_buf_cstr(&result.msgctxt));
    NAOS_CHECK_STR_EQ("Hello, world", mnd_buf_cstr(&result.msgid));
    NAOS_CHECK_FALSE(result.has_msgid_plural);
    CHECK_STRINGS(&result.msgstr, msgstr, 1);
    CHECK_STRINGS(&result.translator_comments, translator, 1);
    CHECK_STRINGS(&result.extracted_comments, extracted, 1);
    NAOS_REQUIRE_UINT_EQ(2, result.refs.count);
    NAOS_CHECK_STR_EQ("new.c", mnd_buf_cstr(&result.refs.items[0].file));
    NAOS_CHECK_UINT_EQ(10, result.refs.items[0].line);
    NAOS_CHECK_STR_EQ("other.c", mnd_buf_cstr(&result.refs.items[1].file));
    NAOS_CHECK_UINT_EQ(0, result.refs.items[1].line);
    /* 強制で fuzzy。d のフラグ（c-format）は使わず、未知のフラグも r の順に保つ */
    CHECK_STRINGS(&result.flags, flags, 3);
    /* d が fuzzy でないので、前回の値は d の msgctxt と msgid */
    NAOS_CHECK_TRUE(result.has_prev_msgctxt);
    NAOS_CHECK_STR_EQ("menu", mnd_buf_cstr(&result.prev_msgctxt));
    NAOS_CHECK_TRUE(result.has_prev_msgid);
    NAOS_CHECK_STR_EQ("Hello world", mnd_buf_cstr(&result.prev_msgid));
    NAOS_CHECK_FALSE(result.has_prev_msgid_plural);
    NAOS_CHECK_FALSE(result.obsolete);
    NAOS_CHECK_UINT_EQ(0, result.line);

    mnd_po_entry_free(&result);
}

NAOS_TEST(entry_combine_exact_match_of_obsolete_entry)
{
    static const char *const flags[] = { "c-format" };
    static const char *const msgstr[] = { "ファイル" };
    mnd_po_entry_t result;

    /* 廃止エントリとの完全一致: fuzzy にならず、廃止エントリでもなく、行番号は 0 */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "#, x-old\n"
        "#~ msgid \"File\"\n"
        "#~ msgstr \"ファイル\"\n",
        "#, c-format\n"
        "msgid \"File\"\n"
        "msgstr \"\"\n",
        false, &result);

    NAOS_CHECK_FALSE(result.has_msgctxt);
    NAOS_CHECK_STR_EQ("File", mnd_buf_cstr(&result.msgid));
    CHECK_STRINGS(&result.msgstr, msgstr, 1);
    CHECK_STRINGS(&result.flags, flags, 1);
    NAOS_CHECK_FALSE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_CHECK_UINT_EQ(0, result.translator_comments.count);
    NAOS_CHECK_UINT_EQ(0, result.extracted_comments.count);
    NAOS_CHECK_UINT_EQ(0, result.refs.count);
    /* 前回の値は d の msgid（d に msgctxt がないので前回の msgctxt もない）。整理は手順 6 で行う */
    NAOS_CHECK_FALSE(result.has_prev_msgctxt);
    NAOS_CHECK_TRUE(result.has_prev_msgid);
    NAOS_CHECK_STR_EQ("File", mnd_buf_cstr(&result.prev_msgid));
    NAOS_CHECK_FALSE(result.has_prev_msgid_plural);
    NAOS_CHECK_FALSE(result.obsolete);
    NAOS_CHECK_UINT_EQ(0, result.line);

    mnd_po_entry_free(&result);
}

NAOS_TEST(entry_combine_flag_order)
{
    static const char *const fuzzy_flags[] = { "fuzzy", "x-b", "c-format", "x-a" };
    static const char *const plain_flags[] = { "x-b", "c-format", "x-a" };
    mnd_po_entry_t result;

    /* d が fuzzy: fuzzy を先頭に、r のフラグから fuzzy を除いたものを r の順に */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "#, x-old, fuzzy\n"
        "msgid \"Open\"\n"
        "msgstr \"開く\"\n",
        "#, x-b, fuzzy, c-format, x-a\n"
        "msgid \"Open\"\n"
        "msgstr \"\"\n",
        false, &result);
    CHECK_STRINGS(&result.flags, fuzzy_flags, 4);
    mnd_po_entry_free(&result);

    /* d が fuzzy でなく強制もなければ、r の fuzzy は無視する */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "msgid \"Open\"\n"
        "msgstr \"開く\"\n",
        "#, x-b, fuzzy, c-format, x-a\n"
        "msgid \"Open\"\n"
        "msgstr \"\"\n",
        false, &result);
    CHECK_STRINGS(&result.flags, plain_flags, 3);
    mnd_po_entry_free(&result);
}

NAOS_TEST(entry_combine_previous_of_fuzzy_old_entry)
{
    mnd_po_entry_t result;

    /* d が fuzzy なら、前回の値は d の前回の値（ないものはない） */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "#, fuzzy\n"
        "#| msgctxt \"prev context\"\n"
        "#| msgid \"Save file\"\n"
        "msgctxt \"context\"\n"
        "msgid \"Save\"\n"
        "msgstr \"保存\"\n",
        "msgctxt \"context\"\n"
        "msgid \"Save\"\n"
        "msgstr \"\"\n",
        false, &result);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_CHECK_TRUE(result.has_prev_msgctxt);
    NAOS_CHECK_STR_EQ("prev context", mnd_buf_cstr(&result.prev_msgctxt));
    NAOS_CHECK_TRUE(result.has_prev_msgid);
    NAOS_CHECK_STR_EQ("Save file", mnd_buf_cstr(&result.prev_msgid));
    NAOS_CHECK_FALSE(result.has_prev_msgid_plural);
    mnd_po_entry_free(&result);

    /* fuzzy の d に前回の値がなければ、強制の fuzzy でも前回の値はない */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "#, fuzzy\n"
        "msgctxt \"context\"\n"
        "msgid \"Save\"\n"
        "msgstr \"保存\"\n",
        "msgid \"Save!\"\n"
        "msgstr \"\"\n",
        true, &result);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_CHECK_FALSE(result.has_prev_msgctxt);
    NAOS_CHECK_FALSE(result.has_prev_msgid);
    NAOS_CHECK_FALSE(result.has_prev_msgid_plural);
    mnd_po_entry_free(&result);
}

NAOS_TEST(entry_combine_previous_of_plain_old_entry)
{
    static const char *const msgstr[] = { "%d 個のファイル", "%d 個のファイルたち" };
    mnd_po_entry_t result;

    /* d が fuzzy でなければ、前回の値は d の msgctxt・msgid・msgid_plural（d の前回の値は使わない） */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "#| msgid \"ignored\"\n"
        "msgid \"%d file\"\n"
        "msgid_plural \"%d files\"\n"
        "msgstr[0] \"%d 個のファイル\"\n"
        "msgstr[1] \"%d 個のファイルたち\"\n",
        "msgid \"%d file\"\n"
        "msgid_plural \"%d files\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n",
        false, &result);
    NAOS_CHECK_FALSE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_CHECK_TRUE(result.has_msgid_plural);
    NAOS_CHECK_STR_EQ("%d files", mnd_buf_cstr(&result.msgid_plural));
    CHECK_STRINGS(&result.msgstr, msgstr, 2);
    NAOS_CHECK_FALSE(result.has_prev_msgctxt);
    NAOS_CHECK_TRUE(result.has_prev_msgid);
    NAOS_CHECK_STR_EQ("%d file", mnd_buf_cstr(&result.prev_msgid));
    NAOS_CHECK_TRUE(result.has_prev_msgid_plural);
    NAOS_CHECK_STR_EQ("%d files", mnd_buf_cstr(&result.prev_msgid_plural));
    mnd_po_entry_free(&result);
}

NAOS_TEST(entry_combine_plural_mismatch_is_fuzzy)
{
    static const char *const msgstr[] = { "%d 個", "%d 個たち" };
    mnd_po_entry_t result;

    /* 両方が複数形で msgid_plural が異なれば fuzzy。msgstr[n] はそのまま */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"%d 個\"\n"
        "msgstr[1] \"%d 個たち\"\n",
        "msgid \"%d item\"\n"
        "msgid_plural \"%d entries\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n",
        false, &result);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_CHECK_STR_EQ("%d entries", mnd_buf_cstr(&result.msgid_plural));
    CHECK_STRINGS(&result.msgstr, msgstr, 2);
    mnd_po_entry_free(&result);

    /* 片方だけが複数形なら fuzzy */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "msgid \"%d item\"\n"
        "msgstr \"%d 個\"\n",
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n",
        false, &result);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_CHECK_TRUE(result.has_msgid_plural);
    mnd_po_entry_free(&result);

    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"%d 個\"\n"
        "msgstr[1] \"%d 個たち\"\n",
        "msgid \"%d item\"\n"
        "msgstr \"\"\n",
        false, &result);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_CHECK_FALSE(result.has_msgid_plural);
    mnd_po_entry_free(&result);
}

NAOS_TEST(entry_combine_untranslated_exact_match)
{
    static const char *const msgstr[] = { "" };
    static const char *const translator[] = { "note" };
    mnd_po_entry_t result;

    /* 未翻訳の完全一致: 空の msgstr を保ち、fuzzy にしない */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS(
        "# note\n"
        "msgid \"Quit\"\n"
        "msgstr \"\"\n",
        "#: main.c:3\n"
        "msgid \"Quit\"\n"
        "msgstr \"\"\n",
        false, &result);
    CHECK_STRINGS(&result.msgstr, msgstr, 1);
    CHECK_STRINGS(&result.translator_comments, translator, 1);
    NAOS_CHECK_UINT_EQ(0, result.flags.count);
    NAOS_REQUIRE_UINT_EQ(1, result.refs.count);
    NAOS_CHECK_STR_EQ("main.c", mnd_buf_cstr(&result.refs.items[0].file));
    NAOS_CHECK_FALSE(mnd_merge_has_translation(&result));
    mnd_po_entry_free(&result);
}

NAOS_TEST(entry_has_translation)
{
    mnd_po_entry_t entry;

    /* msgstr がない */
    mnd_po_entry_init(&entry);
    NAOS_CHECK_FALSE(mnd_merge_has_translation(&entry));

    /* 最初の msgstr が空 */
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "", 0));
    NAOS_CHECK_FALSE(mnd_merge_has_translation(&entry));
    mnd_po_entry_free(&entry);

    /* 最初の msgstr が空でない */
    mnd_po_entry_init(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "x", 1));
    NAOS_CHECK_TRUE(mnd_merge_has_translation(&entry));
    mnd_po_entry_free(&entry);

    /* 複数形で msgstr[0] が空なら、ほかが空でなくても訳がない */
    mnd_po_entry_init(&entry);
    entry.has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry.msgid_plural, "files"));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "", 0));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "y", 1));
    NAOS_CHECK_FALSE(mnd_merge_has_translation(&entry));
    mnd_po_entry_free(&entry);

    /* 複数形で msgstr[0] が空でなければ、ほかが空でも訳がある */
    mnd_po_entry_init(&entry);
    entry.has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry.msgid_plural, "files"));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "x", 1));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "", 0));
    NAOS_CHECK_TRUE(mnd_merge_has_translation(&entry));
    mnd_po_entry_free(&entry);
}

/* buf の内容を 'X' で書き換える */
static void scribble(mnd_buf_t *buf)
{
    if (buf->len > 0)
    {
        memset(buf->data, 'X', buf->len);
    }
}

NAOS_TEST(entry_copy_is_deep)
{
    static const char *const translator[] = { "t1", "t2" };
    static const char *const extracted[] = { "e1" };
    static const char *const flags[] = { "fuzzy", "x-unknown" };
    static const char *const msgstr[] = { "s0", "s1" };
    mnd_po_entry_t src;
    mnd_po_entry_t dst;
    size_t i;

    /* すべての要素を持つエントリ */
    mnd_po_entry_init(&src);
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&src.translator_comments, "t1", 2));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&src.translator_comments, "t2", 2));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&src.extracted_comments, "e1", 2));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&src.refs, "a.c", 12));
    NAOS_REQUIRE_TRUE(mnd_po_refs_push(&src.refs, "b.c", 0));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&src.flags, "fuzzy", 5));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&src.flags, "x-unknown", 9));
    src.has_prev_msgctxt = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&src.prev_msgctxt, "pc"));
    src.has_prev_msgid = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&src.prev_msgid, "pid"));
    src.has_prev_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&src.prev_msgid_plural, "pids"));
    src.has_msgctxt = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&src.msgctxt, "ctx"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&src.msgid, "id"));
    src.has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&src.msgid_plural, "ids"));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&src.msgstr, "s0", 2));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&src.msgstr, "s1", 2));
    src.obsolete = true;
    src.line = 42;

    mnd_po_entry_init(&dst);
    NAOS_REQUIRE_TRUE(mnd_merge_entry_copy(&dst, &src));

    /* 複製後に入力を書き換えて解放しても、複製は変わらない */
    scribble(&src.msgid);
    scribble(&src.msgctxt);
    scribble(&src.prev_msgid);
    for (i = 0; i < src.msgstr.count; i++)
    {
        scribble(&src.msgstr.items[i]);
    }
    for (i = 0; i < src.flags.count; i++)
    {
        scribble(&src.flags.items[i]);
    }
    scribble(&src.refs.items[0].file);
    scribble(&src.translator_comments.items[0]);
    mnd_po_entry_free(&src);

    CHECK_STRINGS(&dst.translator_comments, translator, 2);
    CHECK_STRINGS(&dst.extracted_comments, extracted, 1);
    NAOS_REQUIRE_UINT_EQ(2, dst.refs.count);
    NAOS_CHECK_STR_EQ("a.c", mnd_buf_cstr(&dst.refs.items[0].file));
    NAOS_CHECK_UINT_EQ(12, dst.refs.items[0].line);
    NAOS_CHECK_STR_EQ("b.c", mnd_buf_cstr(&dst.refs.items[1].file));
    NAOS_CHECK_UINT_EQ(0, dst.refs.items[1].line);
    CHECK_STRINGS(&dst.flags, flags, 2);
    NAOS_CHECK_TRUE(dst.has_prev_msgctxt);
    NAOS_CHECK_STR_EQ("pc", mnd_buf_cstr(&dst.prev_msgctxt));
    NAOS_CHECK_TRUE(dst.has_prev_msgid);
    NAOS_CHECK_STR_EQ("pid", mnd_buf_cstr(&dst.prev_msgid));
    NAOS_CHECK_TRUE(dst.has_prev_msgid_plural);
    NAOS_CHECK_STR_EQ("pids", mnd_buf_cstr(&dst.prev_msgid_plural));
    NAOS_CHECK_TRUE(dst.has_msgctxt);
    NAOS_CHECK_STR_EQ("ctx", mnd_buf_cstr(&dst.msgctxt));
    NAOS_CHECK_STR_EQ("id", mnd_buf_cstr(&dst.msgid));
    NAOS_CHECK_TRUE(dst.has_msgid_plural);
    NAOS_CHECK_STR_EQ("ids", mnd_buf_cstr(&dst.msgid_plural));
    CHECK_STRINGS(&dst.msgstr, msgstr, 2);
    NAOS_CHECK_TRUE(dst.obsolete);
    NAOS_CHECK_UINT_EQ(42, dst.line);

    mnd_po_entry_free(&dst);
}

NAOS_TEST(entry_copy_keeps_absent_optional_strings_absent)
{
    mnd_po_entry_t src;
    mnd_po_entry_t dst;

    /* has_* が偽の文字列は、内容があっても複製しない（内容は無視される） */
    mnd_po_entry_init(&src);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&src.msgctxt, "ignored"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&src.msgid, ""));

    mnd_po_entry_init(&dst);
    NAOS_REQUIRE_TRUE(mnd_merge_entry_copy(&dst, &src));
    mnd_po_entry_free(&src);

    NAOS_CHECK_FALSE(dst.has_msgctxt);
    NAOS_CHECK_UINT_EQ(0, dst.msgctxt.len);
    NAOS_CHECK_FALSE(dst.has_msgid_plural);
    NAOS_CHECK_FALSE(dst.has_prev_msgid);
    NAOS_CHECK_STR_EQ("", mnd_buf_cstr(&dst.msgid));
    NAOS_CHECK_UINT_EQ(0, dst.msgstr.count);
    NAOS_CHECK_UINT_EQ(0, dst.refs.count);
    NAOS_CHECK_FALSE(dst.obsolete);

    mnd_po_entry_free(&dst);
}

NAOS_TEST(entry_combine_singular_to_plural)
{
    static const char *const three[] = { "%d 個", "%d 個", "%d 個" };
    static const char *const one[] = { "%d 個" };
    static const char *const empty_two[] = { "", "" };
    mnd_po_entry_t old_entry;
    mnd_po_entry_t template_entry;
    mnd_po_entry_t result;

    /* r が複数形で d が単数形: d の msgstr を nplurals 個に複製して fuzzy */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS_N(
        "msgid \"%d item\"\n"
        "msgstr \"%d 個\"\n",
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n",
        false, 3, &result);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_CHECK_TRUE(result.has_msgid_plural);
    NAOS_CHECK_STR_EQ("%d items", mnd_buf_cstr(&result.msgid_plural));
    CHECK_STRINGS(&result.msgstr, three, 3);
    /* 前回の値は d の値。d に msgid_plural がないので前回の msgid_plural はない（6.5） */
    NAOS_CHECK_TRUE(result.has_prev_msgid);
    NAOS_CHECK_STR_EQ("%d item", mnd_buf_cstr(&result.prev_msgid));
    NAOS_CHECK_FALSE(result.has_prev_msgid_plural);
    mnd_po_entry_free(&result);

    /* nplurals が 1 なら 1 個 */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS_N(
        "msgid \"%d item\"\n"
        "msgstr \"%d 個\"\n",
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n",
        false, 1, &result);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    CHECK_STRINGS(&result.msgstr, one, 1);
    mnd_po_entry_free(&result);

    /* d に msgstr がなければ、空文字列を nplurals 個 */
    mnd_po_entry_init(&old_entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&old_entry.msgid, "%d item"));
    mnd_po_entry_init(&template_entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&template_entry.msgid, "%d item"));
    template_entry.has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&template_entry.msgid_plural, "%d items"));
    mnd_po_entry_init(&result);
    NAOS_REQUIRE_TRUE(mnd_merge_combine(&result, &old_entry, &template_entry, false, 2));
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    CHECK_STRINGS(&result.msgstr, empty_two, 2);
    mnd_po_entry_free(&result);
    mnd_po_entry_free(&template_entry);
    mnd_po_entry_free(&old_entry);
}

NAOS_TEST(entry_combine_plural_to_singular)
{
    static const char *const first[] = { "%d 個" };
    static const char *const empty_one[] = { "" };
    mnd_po_entry_t old_entry;
    mnd_po_entry_t template_entry;
    mnd_po_entry_t result;

    /* r が単数形で d が複数形: d の最初の msgstr だけにして fuzzy（nplurals に依らない） */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS_N(
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"%d 個\"\n"
        "msgstr[1] \"%d 個たち\"\n"
        "msgstr[2] \"%d 個ら\"\n",
        "msgid \"%d item\"\n"
        "msgstr \"\"\n",
        false, 3, &result);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_CHECK_FALSE(result.has_msgid_plural);
    CHECK_STRINGS(&result.msgstr, first, 1);
    /* 前回の値は d の msgid と msgid_plural（6.5） */
    NAOS_CHECK_TRUE(result.has_prev_msgid);
    NAOS_CHECK_STR_EQ("%d item", mnd_buf_cstr(&result.prev_msgid));
    NAOS_CHECK_TRUE(result.has_prev_msgid_plural);
    NAOS_CHECK_STR_EQ("%d items", mnd_buf_cstr(&result.prev_msgid_plural));
    mnd_po_entry_free(&result);

    /* d に msgstr がなければ、空文字列 1 個 */
    mnd_po_entry_init(&old_entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&old_entry.msgid, "%d item"));
    old_entry.has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&old_entry.msgid_plural, "%d items"));
    mnd_po_entry_init(&template_entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&template_entry.msgid, "%d item"));
    mnd_po_entry_init(&result);
    NAOS_REQUIRE_TRUE(mnd_merge_combine(&result, &old_entry, &template_entry, false, 2));
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    CHECK_STRINGS(&result.msgstr, empty_one, 1);
    mnd_po_entry_free(&result);
    mnd_po_entry_free(&template_entry);
    mnd_po_entry_free(&old_entry);
}

NAOS_TEST(entry_combine_plural_keeps_msgstr_count)
{
    static const char *const three[] = { "%d 個", "%d 個たち", "%d 個ら" };
    static const char *const one[] = { "%d 個" };
    mnd_po_entry_t result;

    /* 両方が複数形で msgid_plural が同じ: nplurals と異なる個数でもそのまま、fuzzy にしない（6.1、6.7） */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS_N(
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"%d 個\"\n"
        "msgstr[1] \"%d 個たち\"\n"
        "msgstr[2] \"%d 個ら\"\n",
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n",
        false, 2, &result);
    NAOS_CHECK_FALSE(mnd_po_flag_has(&result, "fuzzy"));
    CHECK_STRINGS(&result.msgstr, three, 3);
    mnd_po_entry_free(&result);

    mnd_po_entry_init(&result);
    COMBINE_TEXTS_N(
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"%d 個\"\n",
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n",
        false, 3, &result);
    NAOS_CHECK_FALSE(mnd_po_flag_has(&result, "fuzzy"));
    CHECK_STRINGS(&result.msgstr, one, 1);
    mnd_po_entry_free(&result);

    /* msgid_plural が異なれば fuzzy で、個数はそのまま。前回の値は d の msgid と msgid_plural（6.4、6.5） */
    mnd_po_entry_init(&result);
    COMBINE_TEXTS_N(
        "msgid \"%d item\"\n"
        "msgid_plural \"%d items\"\n"
        "msgstr[0] \"%d 個\"\n"
        "msgstr[1] \"%d 個たち\"\n"
        "msgstr[2] \"%d 個ら\"\n",
        "msgid \"%d item\"\n"
        "msgid_plural \"%d entries\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n",
        false, 2, &result);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&result, "fuzzy"));
    NAOS_CHECK_STR_EQ("%d entries", mnd_buf_cstr(&result.msgid_plural));
    CHECK_STRINGS(&result.msgstr, three, 3);
    NAOS_CHECK_TRUE(result.has_prev_msgid);
    NAOS_CHECK_STR_EQ("%d item", mnd_buf_cstr(&result.prev_msgid));
    NAOS_CHECK_TRUE(result.has_prev_msgid_plural);
    NAOS_CHECK_STR_EQ("%d items", mnd_buf_cstr(&result.prev_msgid_plural));
    mnd_po_entry_free(&result);
}

/* template_text の最初のエントリから、nplurals を渡して fresh で result（初期化済み）を作る */
static void fresh_text(const char *template_text, size_t nplurals, mnd_po_entry_t *result)
{
    mnd_po_catalog_t template_catalog;

    TEST_READ_PO(template_text, &template_catalog);
    NAOS_REQUIRE_UINT_EQ(1, template_catalog.count);
    NAOS_REQUIRE_TRUE(mnd_merge_fresh(result, &template_catalog.entries[0], nplurals));
    destroy_catalog(&template_catalog);
}

#define FRESH_TEXT(template_text, nplurals, result) \
    do \
    { \
        NAOS_TRACE_PUSH("in FRESH_TEXT"); \
        fresh_text((template_text), (nplurals), (result)); \
        NAOS_TRACE_POP(); \
    } while (0)

NAOS_TEST(entry_fresh_copies_template)
{
    static const char *const translator[] = { "template translator" };
    static const char *const extracted[] = { "note" };
    static const char *const flags[] = { "c-format", "x-unknown" };
    static const char *const msgstr[] = { "" };
    mnd_po_entry_t result;

    /* 新規: r の複製（3.10）。行番号は 0、廃止エントリではない */
    mnd_po_entry_init(&result);
    FRESH_TEXT(
        "\n"
        "# template translator\n"
        "#. note\n"
        "#: main.c:7\n"
        "#, c-format, x-unknown\n"
        "msgctxt \"menu\"\n"
        "msgid \"Open %s\"\n"
        "msgstr \"\"\n",
        3, &result);
    CHECK_STRINGS(&result.translator_comments, translator, 1);
    CHECK_STRINGS(&result.extracted_comments, extracted, 1);
    NAOS_REQUIRE_UINT_EQ(1, result.refs.count);
    NAOS_CHECK_STR_EQ("main.c", mnd_buf_cstr(&result.refs.items[0].file));
    NAOS_CHECK_UINT_EQ(7, result.refs.items[0].line);
    CHECK_STRINGS(&result.flags, flags, 2);
    NAOS_CHECK_TRUE(result.has_msgctxt);
    NAOS_CHECK_STR_EQ("menu", mnd_buf_cstr(&result.msgctxt));
    NAOS_CHECK_STR_EQ("Open %s", mnd_buf_cstr(&result.msgid));
    NAOS_CHECK_FALSE(result.has_msgid_plural);
    /* 単数形は nplurals に依らず msgstr をそのまま */
    CHECK_STRINGS(&result.msgstr, msgstr, 1);
    NAOS_CHECK_FALSE(result.obsolete);
    NAOS_CHECK_UINT_EQ(0, result.line);
    mnd_po_entry_free(&result);
}

NAOS_TEST(entry_fresh_plural_msgstr_count)
{
    static const char *const empty_three[] = { "", "", "" };
    static const char *const empty_one[] = { "" };
    static const char *const partial[] = { "", "x" };
    mnd_po_entry_t template_entry;
    mnd_po_entry_t result;

    /* 複数形で msgstr がすべて空なら、空文字列 nplurals 個（6.6） */
    mnd_po_entry_init(&result);
    FRESH_TEXT(
        "msgid \"%d file\"\n"
        "msgid_plural \"%d files\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n",
        3, &result);
    NAOS_CHECK_TRUE(result.has_msgid_plural);
    CHECK_STRINGS(&result.msgstr, empty_three, 3);
    NAOS_CHECK_FALSE(mnd_po_flag_has(&result, "fuzzy"));
    mnd_po_entry_free(&result);

    mnd_po_entry_init(&result);
    FRESH_TEXT(
        "msgid \"%d file\"\n"
        "msgid_plural \"%d files\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"\"\n",
        1, &result);
    CHECK_STRINGS(&result.msgstr, empty_one, 1);
    mnd_po_entry_free(&result);

    /* 空でない msgstr があれば、個数を変えない（6.7） */
    mnd_po_entry_init(&result);
    FRESH_TEXT(
        "msgid \"%d file\"\n"
        "msgid_plural \"%d files\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"x\"\n",
        3, &result);
    CHECK_STRINGS(&result.msgstr, partial, 2);
    mnd_po_entry_free(&result);

    /* msgstr が 1 個もない複数形も「すべて空」とする（msgmerge の判定と同じく、空でないものがない） */
    mnd_po_entry_init(&template_entry);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&template_entry.msgid, "%d file"));
    template_entry.has_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&template_entry.msgid_plural, "%d files"));
    template_entry.obsolete = true;
    template_entry.line = 9;
    mnd_po_entry_init(&result);
    NAOS_REQUIRE_TRUE(mnd_merge_fresh(&result, &template_entry, 3));
    CHECK_STRINGS(&result.msgstr, empty_three, 3);
    NAOS_CHECK_FALSE(result.obsolete);
    NAOS_CHECK_UINT_EQ(0, result.line);
    mnd_po_entry_free(&result);
    mnd_po_entry_free(&template_entry);
}

NAOS_TEST(entry_obsolete_drops_extracted_comments_and_refs)
{
    static const char *const translator[] = { "old translator" };
    static const char *const flags[] = { "fuzzy", "c-format", "x-unknown" };
    static const char *const msgstr[] = { "%d 個のファイル", "%d 個のファイルたち" };
    mnd_po_catalog_t old_catalog;
    mnd_po_entry_t result;

    /* 抽出コメントと参照だけを消し、ほかの要素を保つ（4.2）。廃止エントリで、行番号は 0 */
    TEST_READ_PO(
        "\n"
        "# old translator\n"
        "#. old note\n"
        "#: old.c:1 other.c:2\n"
        "#, fuzzy, c-format, x-unknown\n"
        "#| msgctxt \"prev\"\n"
        "#| msgid \"%d old file\"\n"
        "#| msgid_plural \"%d old files\"\n"
        "msgctxt \"ctx\"\n"
        "msgid \"%d file\"\n"
        "msgid_plural \"%d files\"\n"
        "msgstr[0] \"%d 個のファイル\"\n"
        "msgstr[1] \"%d 個のファイルたち\"\n",
        &old_catalog);
    NAOS_REQUIRE_UINT_EQ(1, old_catalog.count);
    NAOS_REQUIRE_TRUE(old_catalog.entries[0].line != 0);

    mnd_po_entry_init(&result);
    NAOS_REQUIRE_TRUE(mnd_merge_obsolete(&result, &old_catalog.entries[0]));
    destroy_catalog(&old_catalog);

    CHECK_STRINGS(&result.translator_comments, translator, 1);
    NAOS_CHECK_UINT_EQ(0, result.extracted_comments.count);
    NAOS_CHECK_UINT_EQ(0, result.refs.count);
    CHECK_STRINGS(&result.flags, flags, 3);
    NAOS_CHECK_TRUE(result.has_prev_msgctxt);
    NAOS_CHECK_STR_EQ("prev", mnd_buf_cstr(&result.prev_msgctxt));
    NAOS_CHECK_TRUE(result.has_prev_msgid);
    NAOS_CHECK_STR_EQ("%d old file", mnd_buf_cstr(&result.prev_msgid));
    NAOS_CHECK_TRUE(result.has_prev_msgid_plural);
    NAOS_CHECK_STR_EQ("%d old files", mnd_buf_cstr(&result.prev_msgid_plural));
    NAOS_CHECK_TRUE(result.has_msgctxt);
    NAOS_CHECK_STR_EQ("ctx", mnd_buf_cstr(&result.msgctxt));
    NAOS_CHECK_STR_EQ("%d file", mnd_buf_cstr(&result.msgid));
    NAOS_CHECK_TRUE(result.has_msgid_plural);
    NAOS_CHECK_STR_EQ("%d files", mnd_buf_cstr(&result.msgid_plural));
    CHECK_STRINGS(&result.msgstr, msgstr, 2);
    NAOS_CHECK_TRUE(result.obsolete);
    NAOS_CHECK_UINT_EQ(0, result.line);
    mnd_po_entry_free(&result);

    /* 元が廃止エントリでも同じ */
    TEST_READ_PO(
        "#~ msgid \"Close\"\n"
        "#~ msgstr \"閉じる\"\n",
        &old_catalog);
    NAOS_REQUIRE_UINT_EQ(1, old_catalog.count);
    mnd_po_entry_init(&result);
    NAOS_REQUIRE_TRUE(mnd_merge_obsolete(&result, &old_catalog.entries[0]));
    destroy_catalog(&old_catalog);
    NAOS_CHECK_STR_EQ("Close", mnd_buf_cstr(&result.msgid));
    NAOS_REQUIRE_UINT_EQ(1, result.msgstr.count);
    NAOS_CHECK_STR_EQ("閉じる", mnd_buf_cstr(&result.msgstr.items[0]));
    NAOS_CHECK_TRUE(result.obsolete);
    NAOS_CHECK_UINT_EQ(0, result.line);
    mnd_po_entry_free(&result);
}

/* entry に前回の msgctxt・msgid・msgid_plural をすべて設定する */
static void set_previous(mnd_po_entry_t *entry)
{
    entry->has_prev_msgctxt = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgctxt, "pc"));
    entry->has_prev_msgid = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid, "pid"));
    entry->has_prev_msgid_plural = true;
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&entry->prev_msgid_plural, "pids"));
}

/* entry に前回の値が 1 つもないことを調べる */
static void check_no_previous(const mnd_po_entry_t *entry)
{
    NAOS_CHECK_FALSE(entry->has_prev_msgctxt);
    NAOS_CHECK_FALSE(entry->has_prev_msgid);
    NAOS_CHECK_FALSE(entry->has_prev_msgid_plural);
}

NAOS_TEST(entry_tidy_previous)
{
    mnd_po_entry_t entry;

    /* fuzzy で訳があれば保つ */
    mnd_po_entry_init(&entry);
    set_previous(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "fuzzy"));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "x", 1));
    mnd_merge_tidy_previous(&entry);
    NAOS_CHECK_TRUE(entry.has_prev_msgctxt);
    NAOS_CHECK_STR_EQ("pc", mnd_buf_cstr(&entry.prev_msgctxt));
    NAOS_CHECK_TRUE(entry.has_prev_msgid);
    NAOS_CHECK_STR_EQ("pid", mnd_buf_cstr(&entry.prev_msgid));
    NAOS_CHECK_TRUE(entry.has_prev_msgid_plural);
    NAOS_CHECK_STR_EQ("pids", mnd_buf_cstr(&entry.prev_msgid_plural));
    mnd_po_entry_free(&entry);

    /* fuzzy でなければ消す（2.5） */
    mnd_po_entry_init(&entry);
    set_previous(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "x", 1));
    mnd_merge_tidy_previous(&entry);
    check_no_previous(&entry);
    NAOS_CHECK_UINT_EQ(1, entry.msgstr.count);
    mnd_po_entry_free(&entry);

    /* fuzzy でも最初の msgstr が空なら消す */
    mnd_po_entry_init(&entry);
    set_previous(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "fuzzy"));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "", 0));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry.msgstr, "y", 1));
    mnd_merge_tidy_previous(&entry);
    check_no_previous(&entry);
    NAOS_CHECK_TRUE(mnd_po_flag_has(&entry, "fuzzy"));
    mnd_po_entry_free(&entry);

    /* fuzzy でも msgstr がなければ消す */
    mnd_po_entry_init(&entry);
    set_previous(&entry);
    NAOS_REQUIRE_TRUE(mnd_po_flag_add(&entry, "fuzzy"));
    mnd_merge_tidy_previous(&entry);
    check_no_previous(&entry);
    mnd_po_entry_free(&entry);
}

NAOS_SUITE(entry,
    NAOS_ADD_TEST(entry_scaffold_default_options_enable_fuzzy_matching),
    NAOS_ADD_TEST(entry_combine_takes_elements_by_table),
    NAOS_ADD_TEST(entry_combine_exact_match_of_obsolete_entry),
    NAOS_ADD_TEST(entry_combine_flag_order),
    NAOS_ADD_TEST(entry_combine_previous_of_fuzzy_old_entry),
    NAOS_ADD_TEST(entry_combine_previous_of_plain_old_entry),
    NAOS_ADD_TEST(entry_combine_plural_mismatch_is_fuzzy),
    NAOS_ADD_TEST(entry_combine_untranslated_exact_match),
    NAOS_ADD_TEST(entry_has_translation),
    NAOS_ADD_TEST(entry_copy_is_deep),
    NAOS_ADD_TEST(entry_copy_keeps_absent_optional_strings_absent),
    NAOS_ADD_TEST(entry_combine_singular_to_plural),
    NAOS_ADD_TEST(entry_combine_plural_to_singular),
    NAOS_ADD_TEST(entry_combine_plural_keeps_msgstr_count),
    NAOS_ADD_TEST(entry_fresh_copies_template),
    NAOS_ADD_TEST(entry_fresh_plural_msgstr_count),
    NAOS_ADD_TEST(entry_obsolete_drops_extracted_comments_and_refs),
    NAOS_ADD_TEST(entry_tidy_previous)
)
