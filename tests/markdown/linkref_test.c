/* 参照の解決（6.x）: ラベルの正規化と定義の表 */

#include <naos/naos.h>

#include "test_alloc.h"
#include "tree_check.h"

#include "foundation/diag.h"
#include "markdown/md.h"
#include "markdown/md_internal.h"
#include "markdown/md_linkref.h"
#include "markdown/md_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* メモリ不足のテストで、失敗させる位置を変える回数の上限（成功するまでの確保の回数より十分に大きい） */
#define OOM_MAX_STEPS 1000

/* 解析が成功し、診断が増えず、tree_check を満たすことを確かめる。失敗なら NULL */
static mnd_md_doc_t *parse_ok(const char *data)
{
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;
    bool ok = false;

    ok = mnd_md_parse(data, strlen(data), &options, &doc, &diags);
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
    if (!ok)
    {
        return NULL;
    }
    NAOS_CHECK_NOT_NULL(doc);
    if (doc != NULL)
    {
        NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
    }

    return doc;
}

/* text を正規化した結果が expected と一致することを確かめる */
static void check_normalize(const char *text, size_t len, const char *expected)
{
    mnd_md_array_t out;

    mnd_md_array_init(&out, 1, NULL);
    NAOS_CHECK_TRUE(mnd_md_linkref_normalize(text, len, &out));
    NAOS_CHECK_MEM_EQ(out.items, out.count, expected, strlen(expected));
    mnd_md_array_free(&out);
}

/* label（正規化の前のラベル）で表を引く。見つからなければ NULL */
static const mnd_md_node_t *lookup(const mnd_md_doc_t *doc, const char *label)
{
    const mnd_md_node_t *def = NULL;
    mnd_md_array_t norm;

    mnd_md_array_init(&norm, 1, NULL);
    NAOS_CHECK_TRUE(mnd_md_linkref_normalize(label, strlen(label), &norm));
    def = mnd_md_linkref_lookup(doc, (const char *)norm.items, norm.count);
    mnd_md_array_free(&norm);

    return def;
}

/* def のリンク先の範囲が data の中で expected を指すことを確かめる */
static void check_dest(const mnd_md_doc_t *doc, const mnd_md_node_t *def, const char *expected)
{
    const char *data = mnd_md_doc_data(doc);

    NAOS_REQUIRE_NOT_NULL(def);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_LINK_REF_DEF, (int)def->kind);
    NAOS_CHECK_MEM_EQ(data + def->as.link_ref_def.dest.start,
                      def->as.link_ref_def.dest.end - def->as.link_ref_def.dest.start,
                      expected, strlen(expected));
}

/* 6.2: 前後の空白の除去、連続する空白（タブ・改行・Unicode の空白を含む）のまとめ、大文字小文字の畳み込み */
NAOS_TEST(linkref_normalize_whitespace_and_case)
{
    check_normalize("Foo", 3, "foo");
    check_normalize("  Foo \t\n  BAR  ", 15, "foo bar");
    check_normalize("a\xE3\x80\x80\xC2\xA0" "b", 7, "a b");     /* U+3000 と U+00A0 */
    check_normalize("\xE1\xBA\x9E", 3, "ss");                    /* ẞ（U+1E9E） */
    check_normalize("SS", 2, "ss");
    check_normalize("\xCE\x91\xCE\x93\xCE\xA9", 6, "\xCE\xB1\xCE\xB3\xCF\x89"); /* ΑΓΩ → αγω */
    check_normalize("", 0, "");
    check_normalize(" \n\t", 3, "");
    /* エスケープは正規化しない（論理的な文字列のまま） */
    check_normalize("a\\!", 3, "a\\!");
    /* NUL は U+FFFD として扱う */
    check_normalize("a\0b", 3, "a\xEF\xBF\xBD" "b");
}

/* 6.2: `[ẞ]` と `[SS]` の定義は同じラベルになり、表には最初の定義だけが残る */
NAOS_TEST(linkref_sharp_s_matches_ss)
{
    mnd_md_doc_t *doc = parse_ok("[\xE1\xBA\x9E]: /a\n\n[SS]: /b\n");
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *first = NULL;
    const mnd_md_node_t *second = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    first = root->first_child;
    NAOS_REQUIRE_NOT_NULL(first);
    second = first->next;
    NAOS_REQUIRE_NOT_NULL(second);
    NAOS_CHECK_INT_EQ((int)MND_MD_LINK_REF_DEF, (int)first->kind);
    NAOS_CHECK_INT_EQ((int)MND_MD_LINK_REF_DEF, (int)second->kind);
    NAOS_REQUIRE_NOT_NULL(first->as.link_ref_def.norm_label);
    NAOS_REQUIRE_NOT_NULL(second->as.link_ref_def.norm_label);
    NAOS_CHECK_STR_EQ("ss", first->as.link_ref_def.norm_label);
    NAOS_CHECK_STR_EQ("ss", second->as.link_ref_def.norm_label);
    NAOS_CHECK_PTR_EQ(first, lookup(doc, "ss"));
    NAOS_CHECK_PTR_EQ(first, lookup(doc, "\xE1\xBA\x9E"));
    NAOS_CHECK_PTR_EQ(first, lookup(doc, "sS"));
    check_dest(doc, lookup(doc, "SS"), "/a");
    mnd_md_doc_free(doc);
}

/* 6.3: 同じラベルの 2 つの定義は、どちらもノードとして残り、表には最初のものが残る */
NAOS_TEST(linkref_duplicate_label_keeps_first)
{
    mnd_md_doc_t *doc = parse_ok("[Foo  Bar]: /first\n[foo bar]: /second\n\n> [FOO\n> bar]: /third\n");
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *quote = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_NOT_NULL(root->first_child);
    NAOS_REQUIRE_NOT_NULL(root->first_child->next);
    quote = root->first_child->next->next;
    NAOS_REQUIRE_NOT_NULL(quote);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_BLOCK_QUOTE, (int)quote->kind);
    NAOS_REQUIRE_NOT_NULL(quote->first_child);
    NAOS_CHECK_INT_EQ((int)MND_MD_LINK_REF_DEF, (int)quote->first_child->kind);
    NAOS_CHECK_STR_EQ("foo bar", quote->first_child->as.link_ref_def.norm_label);
    NAOS_CHECK_PTR_EQ(root->first_child, lookup(doc, "FOO BAR"));
    check_dest(doc, lookup(doc, " foo\nbar "), "/first");
    mnd_md_doc_free(doc);
}

/* 6.1: 表には文書のすべての定義が入る（コンテナの中を含む）。ないラベルは NULL */
NAOS_TEST(linkref_table_holds_all_definitions)
{
    mnd_md_doc_t *doc = parse_ok("para [a]\n\n- [a]: /in-item\n\n> [b]: /in-quote\n\n[c]: </angle>\n");

    NAOS_REQUIRE_NOT_NULL(doc);
    check_dest(doc, lookup(doc, "a"), "/in-item");
    check_dest(doc, lookup(doc, "B"), "/in-quote");
    check_dest(doc, lookup(doc, "c"), "/angle");
    NAOS_CHECK_NULL(lookup(doc, "d"));
    NAOS_CHECK_NULL(lookup(doc, ""));
    mnd_md_doc_free(doc);

    /* 定義のない文書でも引ける */
    doc = parse_ok("no definitions\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_NULL(lookup(doc, "a"));
    mnd_md_doc_free(doc);
}

/* 多くの定義（表の伸長）でも、すべて最初の定義を引ける */
NAOS_TEST(linkref_table_grows)
{
    char data[8192];
    char label[16];
    size_t len = 0;
    int i = 0;
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *def = NULL;

    for (i = 0; i < 300; i++)
    {
        len += (size_t)snprintf(data + len, sizeof(data) - len, "[L%d]: /u%d\n", i, i);
    }
    /* 重複（後ろの定義は表に入らない） */
    len += (size_t)snprintf(data + len, sizeof(data) - len, "[l7]: /dup\n");
    NAOS_REQUIRE_UINT_LT(len, sizeof(data));
    doc = parse_ok(data);
    NAOS_REQUIRE_NOT_NULL(doc);
    for (i = 0; i < 300; i++)
    {
        char dest[16];

        (void)snprintf(label, sizeof(label), "l%d", i);
        (void)snprintf(dest, sizeof(dest), "/u%d", i);
        def = lookup(doc, label);
        check_dest(doc, def, dest);
    }
    mnd_md_doc_free(doc);
}

/* メモリ不足: 定義を含む文書の解析が、どの確保で失敗しても漏れなく false を返す */
NAOS_TEST(linkref_out_of_memory)
{
    static const char data[] = "[a]: /u 'title'\n[b\nc]: </v>\n(t\nu)\npara\n\n> [A]: /w\n> ===\n";
    size_t fail_at = 0;
    size_t failures = 0;
    bool succeeded = false;

    for (fail_at = 1; (fail_at <= OOM_MAX_STEPS) && !succeeded; fail_at++)
    {
        mnd_md_test_alloc_t ta;
        mnd_md_options_t options = MND_MD_OPTIONS_INIT;
        mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
        mnd_md_doc_t *doc = (mnd_md_doc_t *)&options;

        mnd_md_test_alloc_init(&ta, fail_at);
        options.allocator = &ta.allocator;
        if (mnd_md_parse(data, sizeof(data) - 1, &options, &doc, &diags))
        {
            succeeded = true;
            NAOS_CHECK_UINT_EQ(0, ta.failures);
            NAOS_REQUIRE_NOT_NULL(doc);
            NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
            NAOS_CHECK_NOT_NULL(lookup(doc, "a"));
            NAOS_CHECK_NOT_NULL(lookup(doc, "b c"));
            mnd_md_doc_free(doc);
        }
        else
        {
            failures++;
            NAOS_CHECK_NULL(doc);
            NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
            NAOS_CHECK_STR_EQ("out of memory", mnd_diag_list_at(&diags, 0)->message);
        }
        NAOS_CHECK_UINT_EQ(0, mnd_md_test_alloc_live(&ta));
        mnd_diag_list_free(&diags);
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_GT(failures, 0);
}

NAOS_SUITE(linkref,
    NAOS_ADD_TEST(linkref_normalize_whitespace_and_case),
    NAOS_ADD_TEST(linkref_sharp_s_matches_ss),
    NAOS_ADD_TEST(linkref_duplicate_label_keeps_first),
    NAOS_ADD_TEST(linkref_table_holds_all_definitions),
    NAOS_ADD_TEST(linkref_table_grows),
    NAOS_ADD_TEST(linkref_out_of_memory)
)
