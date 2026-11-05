/* ブロックの種類と属性（2.x） */

#include <naos/naos.h>

#include "tree_check.h"

#include "foundation/diag.h"
#include "markdown/md.h"
#include "markdown/md_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

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

/* node の子の数 */
static size_t child_count(const mnd_md_node_t *node)
{
    const mnd_md_node_t *child = NULL;
    size_t count = 0;

    for (child = node->first_child; child != NULL; child = child->next)
    {
        count++;
    }

    return count;
}

/* node の index 番目（0 起点）の子。なければ NULL */
static const mnd_md_node_t *child_at(const mnd_md_node_t *node, size_t index)
{
    const mnd_md_node_t *child = node->first_child;
    size_t i = 0;

    for (i = 0; (i < index) && (child != NULL); i++)
    {
        child = child->next;
    }

    return child;
}

/* 1 つの項目を持つ 1 つのリストの文書で、リストの属性と項目の記号の範囲を確かめる */
static void check_single_item_list(const char *data, bool ordered, char marker, unsigned long start,
                                   size_t marker_end)
{
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *list = NULL;
    const mnd_md_node_t *item = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(1, child_count(root));
    list = root->first_child;
    NAOS_CHECK_INT_EQ((int)MND_MD_LIST, (int)list->kind);
    NAOS_CHECK_TRUE(list->as.list.ordered == ordered);
    NAOS_CHECK_INT_EQ((int)marker, (int)list->as.list.marker);
    NAOS_CHECK_UINT_EQ(start, list->as.list.start);
    NAOS_CHECK_TRUE(list->as.list.tight);
    NAOS_REQUIRE_UINT_EQ(1, child_count(list));
    item = list->first_child;
    NAOS_CHECK_INT_EQ((int)MND_MD_ITEM, (int)item->kind);
    NAOS_CHECK_UINT_EQ(0, item->as.item.marker.start);
    NAOS_CHECK_UINT_EQ(marker_end, item->as.item.marker.end);
    NAOS_REQUIRE_UINT_EQ(1, child_count(item));
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)item->first_child->kind);
    mnd_md_doc_free(doc);
}

/* data の最初のリストのタイトかルーズかを確かめる */
static void check_list_tight(const char *data, bool tight)
{
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *list = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    list = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(list);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_LIST, (int)list->kind);
    NAOS_CHECK_TRUE(list->as.list.tight == tight);
    mnd_md_doc_free(doc);
}

/* 2.4: 各リスト記号 */
NAOS_TEST(block_list_marker_kinds)
{
    check_single_item_list("- a\n", false, '-', 0, 1);
    check_single_item_list("+ a\n", false, '+', 0, 1);
    check_single_item_list("* a\n", false, '*', 0, 1);
    check_single_item_list("1. a\n", true, '.', 1, 2);
    check_single_item_list("1) a\n", true, ')', 1, 2);
}

/* 2.4: 開始番号 0 と 123456789 */
NAOS_TEST(block_list_start_numbers)
{
    check_single_item_list("0. a\n", true, '.', 0, 2);
    check_single_item_list("123456789) a\n", true, ')', 123456789UL, 10);
}

/* 2.4: 記号の種類が変わるとリストを分ける。番号が続かなくても同じリスト */
NAOS_TEST(block_list_split_by_marker)
{
    mnd_md_doc_t *doc = parse_ok("- a\n+ b\n1. c\n3. d\n1) e\n");
    const mnd_md_node_t *root = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(4, child_count(root));
    NAOS_CHECK_INT_EQ((int)'-', (int)child_at(root, 0)->as.list.marker);
    NAOS_CHECK_UINT_EQ(1, child_count(child_at(root, 0)));
    NAOS_CHECK_INT_EQ((int)'+', (int)child_at(root, 1)->as.list.marker);
    NAOS_CHECK_INT_EQ((int)'.', (int)child_at(root, 2)->as.list.marker);
    NAOS_CHECK_UINT_EQ(2, child_count(child_at(root, 2)));
    NAOS_CHECK_UINT_EQ(1, child_at(root, 2)->as.list.start);
    NAOS_CHECK_INT_EQ((int)')', (int)child_at(root, 3)->as.list.marker);
    mnd_md_doc_free(doc);
}

/* 2.4: 段落を中断できるリスト項目は、空でなく、順序付きなら開始番号が 1 のもの */
NAOS_TEST(block_list_item_interrupting_paragraph)
{
    mnd_md_doc_t *doc = parse_ok("a\n- b\n");
    const mnd_md_node_t *root = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(2, child_count(root));
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)child_at(root, 0)->kind);
    NAOS_CHECK_INT_EQ((int)MND_MD_LIST, (int)child_at(root, 1)->kind);
    mnd_md_doc_free(doc);

    doc = parse_ok("a\n1. b\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_UINT_EQ(2, child_count(mnd_md_doc_root(doc)));
    mnd_md_doc_free(doc);

    /* 開始番号が 1 でない項目と、空の項目は段落を中断しない */
    doc = parse_ok("a\n2. b\n+\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(1, child_count(root));
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)root->first_child->kind);
    NAOS_CHECK_UINT_EQ(3, root->first_child->line_count);
    mnd_md_doc_free(doc);
}

/* 2.4: タイトとルーズ（空行の位置の違い） */
NAOS_TEST(block_list_tight_and_loose)
{
    check_list_tight("- a\n- b\n", true);
    check_list_tight("- a\n- b\n\n", true);
    check_list_tight("- a\n\n- b\n", false);
    check_list_tight("- a\n\n  b\n", false);
    check_list_tight("- a\n  - b\n\n- c\n", false);
    check_list_tight("- a\n  - b\n\n    c\n- d\n", true);
    check_list_tight("- a\n> b\n", true);
    check_list_tight("1. a\n\n   b\n2. c\n", false);
    check_list_tight("-\n  a\n- b\n", true);
}

/* 入れ子のリストのタイトとルーズは、外側と内側で別に決まる */
NAOS_TEST(block_list_nested_tightness)
{
    mnd_md_doc_t *doc = parse_ok("- a\n  - b\n\n    c\n- d\n");
    const mnd_md_node_t *outer = NULL;
    const mnd_md_node_t *inner = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    outer = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_UINT_EQ(2, child_count(outer));
    NAOS_CHECK_TRUE(outer->as.list.tight);
    NAOS_REQUIRE_UINT_EQ(2, child_count(outer->first_child));
    inner = child_at(outer->first_child, 1);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_LIST, (int)inner->kind);
    NAOS_CHECK_FALSE(inner->as.list.tight);
    NAOS_CHECK_UINT_EQ(2, child_count(inner->first_child));
    mnd_md_doc_free(doc);
}

/* 2.1: 引用、リスト、リスト項目は子のブロックを原文の順に持つ */
NAOS_TEST(block_containers_hold_children_in_order)
{
    mnd_md_doc_t *doc = parse_ok("> a\n>\n> - b\n>   c\n>\n>   d\n> > e\n");
    const mnd_md_node_t *quote = NULL;
    const mnd_md_node_t *list = NULL;
    const mnd_md_node_t *item = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_REQUIRE_UINT_EQ(1, child_count(mnd_md_doc_root(doc)));
    quote = mnd_md_doc_root(doc)->first_child;
    NAOS_CHECK_INT_EQ((int)MND_MD_BLOCK_QUOTE, (int)quote->kind);
    NAOS_REQUIRE_UINT_EQ(3, child_count(quote));
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)child_at(quote, 0)->kind);
    list = child_at(quote, 1);
    NAOS_CHECK_INT_EQ((int)MND_MD_LIST, (int)list->kind);
    NAOS_CHECK_FALSE(list->as.list.tight);
    NAOS_REQUIRE_UINT_EQ(1, child_count(list));
    item = list->first_child;
    NAOS_REQUIRE_UINT_EQ(2, child_count(item));
    NAOS_CHECK_UINT_EQ(2, child_at(item, 0)->line_count);
    NAOS_CHECK_UINT_EQ(1, child_at(item, 1)->line_count);
    NAOS_CHECK_INT_EQ((int)MND_MD_BLOCK_QUOTE, (int)child_at(quote, 2)->kind);
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)child_at(quote, 2)->first_child->kind);
    mnd_md_doc_free(doc);
}

/* リスト項目の内容のインデントに満たない行は、項目を閉じる */
NAOS_TEST(block_list_item_needs_content_indent)
{
    mnd_md_doc_t *doc = parse_ok("1.  a\n\n   b\n");
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *item = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(2, child_count(root));
    item = root->first_child->first_child;
    NAOS_CHECK_UINT_EQ(4, item->as.item.content_indent);
    NAOS_CHECK_UINT_EQ(1, child_count(item));
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)child_at(root, 1)->kind);
    mnd_md_doc_free(doc);

    /* 空のリスト項目のあとの空行で、項目は終わる */
    doc = parse_ok("-\n\n  a\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(2, child_count(root));
    NAOS_CHECK_NULL(root->first_child->first_child->first_child);
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)child_at(root, 1)->kind);
    mnd_md_doc_free(doc);
}

/* 2.9: 引用とリスト項目の遅延継続行は、続いている段落の一部になる */
NAOS_TEST(block_lazy_continuation)
{
    mnd_md_doc_t *doc = parse_ok("> a\nb\n- c\nd\n> - e\nf\n");
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(3, child_count(root));
    para = child_at(root, 0)->first_child;
    NAOS_REQUIRE_INT_EQ((int)MND_MD_PARAGRAPH, (int)para->kind);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    NAOS_CHECK_FALSE(para->lines[0].lazy);
    NAOS_CHECK_TRUE(para->lines[1].lazy);
    para = child_at(root, 1)->first_child->first_child;
    NAOS_REQUIRE_INT_EQ((int)MND_MD_PARAGRAPH, (int)para->kind);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    NAOS_CHECK_TRUE(para->lines[1].lazy);
    para = child_at(root, 2)->first_child->first_child->first_child;
    NAOS_REQUIRE_INT_EQ((int)MND_MD_PARAGRAPH, (int)para->kind);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    NAOS_CHECK_TRUE(para->lines[1].lazy);
    mnd_md_doc_free(doc);

    /* 空行のあとの行は遅延継続行にならない */
    doc = parse_ok("> a\n\nb\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_UINT_EQ(2, child_count(mnd_md_doc_root(doc)));
    mnd_md_doc_free(doc);
}

/* data の根の子の種類が kinds（count 個）と一致することを確かめる */
static void check_root_kinds(const char *data, const mnd_md_kind_t *kinds, size_t count)
{
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *root = NULL;
    size_t i = 0;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_CHECK_UINT_EQ(count, child_count(root));
    for (i = 0; (i < count) && (child_at(root, i) != NULL); i++)
    {
        NAOS_CHECK_INT_EQ((int)kinds[i], (int)child_at(root, i)->kind);
    }
    mnd_md_doc_free(doc);
}

/* 2.2、2.3: ATX 見出しのレベル 1〜6。7 個の `#` は段落になる */
NAOS_TEST(block_atx_heading_levels)
{
    mnd_md_doc_t *doc = parse_ok("# a\n## b\n### c\n#### d\n##### e\n###### f\n####### g\n");
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *heading = NULL;
    int level = 0;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(7, child_count(root));
    for (level = 1; level <= 6; level++)
    {
        heading = child_at(root, (size_t)(level - 1));
        NAOS_CHECK_INT_EQ((int)MND_MD_HEADING, (int)heading->kind);
        NAOS_CHECK_INT_EQ(level, heading->as.heading.level);
        NAOS_CHECK_FALSE(heading->as.heading.setext);
        NAOS_CHECK_UINT_EQ(1, heading->line_count);
    }
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)child_at(root, 6)->kind);
    mnd_md_doc_free(doc);
}

/* 2.3: Setext 見出しの `=`（レベル 1）と `-`（レベル 2）。2 行の内容を持てる */
NAOS_TEST(block_setext_headings)
{
    mnd_md_doc_t *doc = parse_ok("a\n===\nb\nc\n---\n");
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *heading = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(2, child_count(root));
    heading = child_at(root, 0);
    NAOS_CHECK_INT_EQ((int)MND_MD_HEADING, (int)heading->kind);
    NAOS_CHECK_INT_EQ(1, heading->as.heading.level);
    NAOS_CHECK_TRUE(heading->as.heading.setext);
    NAOS_CHECK_UINT_EQ(1, heading->line_count);
    heading = child_at(root, 1);
    NAOS_CHECK_INT_EQ((int)MND_MD_HEADING, (int)heading->kind);
    NAOS_CHECK_INT_EQ(2, heading->as.heading.level);
    NAOS_CHECK_TRUE(heading->as.heading.setext);
    NAOS_CHECK_UINT_EQ(2, heading->line_count);
    mnd_md_doc_free(doc);

    /* 遅延継続行の `===` は下線にならない。段落のない `---` はテーマ区切り */
    doc = parse_ok("> a\n===\n---\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(2, child_count(root));
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)root->first_child->first_child->kind);
    NAOS_CHECK_UINT_EQ(2, root->first_child->first_child->line_count);
    NAOS_CHECK_INT_EQ((int)MND_MD_THEMATIC_BREAK, (int)child_at(root, 1)->kind);
    mnd_md_doc_free(doc);
}

/* 2.2: テーマ区切りは段落を中断し、リスト記号より先に判定する */
NAOS_TEST(block_thematic_breaks)
{
    static const mnd_md_kind_t kinds[] = {
        MND_MD_PARAGRAPH, MND_MD_THEMATIC_BREAK, MND_MD_THEMATIC_BREAK, MND_MD_THEMATIC_BREAK,
        MND_MD_LIST
    };

    check_root_kinds("a\n***\n- - -\n___\n- b\n", kinds, sizeof(kinds) / sizeof(kinds[0]));
}

/* 2.5: バッククォートと `~` のフェンス、長さ 3 と 5、情報文字列、閉じフェンスの有無 */
NAOS_TEST(block_fenced_code)
{
    mnd_md_doc_t *doc = parse_ok("```ruby  x\na\n```\n~~~~~\nb\n~~~\nc\n");
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *code = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(2, child_count(root));
    code = child_at(root, 0);
    NAOS_CHECK_INT_EQ((int)MND_MD_CODE_BLOCK, (int)code->kind);
    NAOS_CHECK_TRUE(code->as.code_block.fenced);
    NAOS_CHECK_INT_EQ((int)'`', (int)code->as.code_block.fence_char);
    NAOS_CHECK_UINT_EQ(3, code->as.code_block.fence_len);
    NAOS_CHECK_UINT_EQ(0, code->as.code_block.fence_indent);
    NAOS_CHECK_TRUE(code->as.code_block.has_info);
    NAOS_CHECK_UINT_EQ(3, code->as.code_block.info.start);
    NAOS_CHECK_UINT_EQ(10, code->as.code_block.info.end);
    NAOS_CHECK_TRUE(code->as.code_block.closed);
    NAOS_CHECK_UINT_EQ(1, code->line_count);

    /* 短い閉じフェンスは閉じない。閉じフェンスがないまま文書が終わる */
    code = child_at(root, 1);
    NAOS_CHECK_INT_EQ((int)MND_MD_CODE_BLOCK, (int)code->kind);
    NAOS_CHECK_TRUE(code->as.code_block.fenced);
    NAOS_CHECK_INT_EQ((int)'~', (int)code->as.code_block.fence_char);
    NAOS_CHECK_UINT_EQ(5, code->as.code_block.fence_len);
    NAOS_CHECK_FALSE(code->as.code_block.has_info);
    NAOS_CHECK_FALSE(code->as.code_block.closed);
    NAOS_CHECK_UINT_EQ(3, code->line_count);
    mnd_md_doc_free(doc);

    /* 内容のないフェンス */
    doc = parse_ok("````\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    code = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(code);
    NAOS_CHECK_INT_EQ((int)MND_MD_CODE_BLOCK, (int)code->kind);
    NAOS_CHECK_UINT_EQ(4, code->as.code_block.fence_len);
    NAOS_CHECK_FALSE(code->as.code_block.closed);
    NAOS_CHECK_UINT_EQ(0, code->line_count);
    mnd_md_doc_free(doc);
}

/* 2.2: インデントコードブロックは段落を中断しない */
NAOS_TEST(block_indented_code)
{
    static const mnd_md_kind_t code_kinds[] = { MND_MD_CODE_BLOCK, MND_MD_PARAGRAPH };
    static const mnd_md_kind_t para_kinds[] = { MND_MD_PARAGRAPH };
    mnd_md_doc_t *doc = parse_ok("    a\n\n    b\nc\n");
    const mnd_md_node_t *code = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    code = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(code);
    NAOS_CHECK_INT_EQ((int)MND_MD_CODE_BLOCK, (int)code->kind);
    NAOS_CHECK_FALSE(code->as.code_block.fenced);
    NAOS_CHECK_UINT_EQ(3, code->line_count);
    mnd_md_doc_free(doc);

    check_root_kinds("    a\nb\n", code_kinds, 2);
    check_root_kinds("a\n    b\n", para_kinds, 1);
}

/* html_block の種類を 1 つ持つ文書であることと、行の数を確かめる */
static void check_html_block(const char *data, int type, size_t line_count)
{
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *html = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    html = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(html);
    NAOS_CHECK_INT_EQ((int)MND_MD_HTML_BLOCK, (int)html->kind);
    NAOS_CHECK_INT_EQ(type, html->as.html_block.type);
    NAOS_CHECK_UINT_EQ(line_count, html->line_count);
    mnd_md_doc_free(doc);
}

/* 2.6: HTML ブロックの開始条件の種類 1〜7。1〜5 は終了条件の行まで、6 と 7 は空行まで */
NAOS_TEST(block_html_block_types)
{
    check_html_block("<script>\nx\n</script>\ny\n", 1, 3);
    check_html_block("<!-- a\n\nb -->\ny\n", 2, 3);
    check_html_block("<?php\n?>\ny\n", 3, 2);
    check_html_block("<!DOCTYPE html>\ny\n", 4, 1);
    check_html_block("<![CDATA[\nx\n]]>\ny\n", 5, 3);
    check_html_block("<div>\n*x*\n\ny\n", 6, 2);
    check_html_block("<custom-tag>\nx\n\ny\n", 7, 2);
}

/* 2.6: 種類 7 は段落を中断せず、種類 1〜6 は中断する */
NAOS_TEST(block_html_block_interrupting_paragraph)
{
    static const mnd_md_kind_t para_kinds[] = { MND_MD_PARAGRAPH };
    static const mnd_md_kind_t quote_kinds[] = { MND_MD_BLOCK_QUOTE };
    static const mnd_md_kind_t html_kinds[] = { MND_MD_PARAGRAPH, MND_MD_HTML_BLOCK };

    check_root_kinds("a\n<custom-tag>\n", para_kinds, 1);
    /* 遅延継続行の候補の段落も中断しない */
    check_root_kinds("> a\n<custom-tag>\n", quote_kinds, 1);
    check_root_kinds("a\n<div>\n", html_kinds, 2);
}

/* 2.2: ATX 見出し・フェンス・HTML ブロックは段落を中断する。コンテナの中のリーフ */
NAOS_TEST(block_leaves_interrupt_paragraph)
{
    static const mnd_md_kind_t kinds[] = {
        MND_MD_PARAGRAPH, MND_MD_HEADING, MND_MD_PARAGRAPH, MND_MD_CODE_BLOCK
    };
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *quote = NULL;

    check_root_kinds("a\n# b\nc\n```\nd\n", kinds, sizeof(kinds) / sizeof(kinds[0]));

    doc = parse_ok("> ```\n> a\n\n- # b\n\n      c\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    quote = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_INT_EQ((int)MND_MD_BLOCK_QUOTE, (int)quote->kind);
    NAOS_REQUIRE_NOT_NULL(quote->first_child);
    NAOS_CHECK_INT_EQ((int)MND_MD_CODE_BLOCK, (int)quote->first_child->kind);
    NAOS_CHECK_UINT_EQ(1, quote->first_child->line_count);
    NAOS_REQUIRE_NOT_NULL(quote->next);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_LIST, (int)quote->next->kind);
    NAOS_REQUIRE_UINT_EQ(2, child_count(quote->next->first_child));
    NAOS_CHECK_INT_EQ((int)MND_MD_HEADING, (int)quote->next->first_child->first_child->kind);
    NAOS_CHECK_INT_EQ((int)MND_MD_CODE_BLOCK, (int)child_at(quote->next->first_child, 1)->kind);
    NAOS_CHECK_FALSE(quote->next->as.list.tight);
    mnd_md_doc_free(doc);
}

/* span が data の中で expected を指すことを確かめる */
static void check_span_text(const char *data, mnd_span_t span, const char *expected)
{
    NAOS_REQUIRE_TRUE(span.start <= span.end);
    NAOS_CHECK_MEM_EQ(data + span.start, span.end - span.start, expected, strlen(expected));
}

/*
 * node がリンク参照定義で、ラベル・リンク先・タイトルの範囲が data の中でそれぞれの文字列を指すことを確かめる
 * （title が NULL ならタイトルがない）
 */
static void check_def(const char *data, const mnd_md_node_t *node, const char *label, const char *dest,
                      bool angle, const char *title, char delim)
{
    const mnd_md_link_ref_def_t *def = NULL;

    NAOS_REQUIRE_NOT_NULL(node);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_LINK_REF_DEF, (int)node->kind);
    def = &node->as.link_ref_def;
    check_span_text(data, def->label, label);
    check_span_text(data, def->dest, dest);
    NAOS_CHECK_TRUE(def->dest_angle == angle);
    NAOS_CHECK_NOT_NULL(def->norm_label);
    NAOS_CHECK_NULL(node->first_child);
    NAOS_REQUIRE_UINT_GT(node->line_count, 0);
    if (title == NULL)
    {
        NAOS_CHECK_FALSE(def->has_title);
    }
    else
    {
        NAOS_CHECK_TRUE(def->has_title);
        check_span_text(data, def->title, title);
        NAOS_CHECK_INT_EQ((int)delim, (int)def->title_delim);
    }
}

/* 2.8: 段落の先頭の定義 2 つは段落の前の兄弟になり、段落の範囲と行は残りの行だけになる */
NAOS_TEST(block_link_ref_defs_before_paragraph)
{
    static const char data[] = "[a]: /u\n[b]: <v w> 'T'\nrest\nmore\n";
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *a = NULL;
    const mnd_md_node_t *b = NULL;
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(3, child_count(root));
    a = child_at(root, 0);
    b = child_at(root, 1);
    para = child_at(root, 2);
    check_def(data, a, "a", "/u", false, NULL, 0);
    NAOS_CHECK_STR_EQ("a", a->as.link_ref_def.norm_label);
    NAOS_CHECK_UINT_EQ(0, a->span.start);
    NAOS_CHECK_UINT_EQ(7, a->span.end);
    NAOS_CHECK_UINT_EQ(1, a->start_line);
    NAOS_CHECK_UINT_EQ(1, a->end_line);
    NAOS_REQUIRE_UINT_EQ(1, a->line_count);
    NAOS_CHECK_UINT_EQ(0, a->lines[0].content.start);
    NAOS_CHECK_UINT_EQ(7, a->lines[0].content.end);
    NAOS_CHECK_UINT_EQ(1, a->lines[0].eol_len);
    check_def(data, b, "b", "v w", true, "T", '\'');
    NAOS_CHECK_UINT_EQ(8, b->span.start);
    NAOS_CHECK_UINT_EQ(22, b->span.end);
    NAOS_CHECK_UINT_EQ(2, b->start_line);
    NAOS_REQUIRE_UINT_EQ(1, b->line_count);
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)para->kind);
    NAOS_CHECK_UINT_EQ(23, para->span.start);
    NAOS_CHECK_UINT_EQ(32, para->span.end);
    NAOS_CHECK_UINT_EQ(3, para->start_line);
    NAOS_CHECK_UINT_EQ(4, para->end_line);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    NAOS_CHECK_UINT_EQ(23, para->lines[0].content.start);
    NAOS_CHECK_UINT_EQ(3, para->lines[0].line);
    NAOS_CHECK_UINT_EQ(28, para->lines[1].content.start);
    mnd_md_doc_free(doc);

    /* 定義は段落の先頭だけ。段落の途中の定義の形の行は段落の一部 */
    doc = parse_ok("para\n[a]: /u\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(1, child_count(root));
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)root->first_child->kind);
    NAOS_CHECK_UINT_EQ(2, root->first_child->line_count);
    mnd_md_doc_free(doc);
}

/* 2.8: 定義だけの段落は取り除かれる（文書、引用、リスト項目の中） */
NAOS_TEST(block_link_ref_defs_only_paragraph)
{
    static const mnd_md_kind_t kinds[] = { MND_MD_LINK_REF_DEF, MND_MD_LINK_REF_DEF, MND_MD_PARAGRAPH };
    static const char quoted[] = "> [a]: /u\n>\n> [b]:\n> /v\n";
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *quote = NULL;
    const mnd_md_node_t *item = NULL;

    check_root_kinds("[a]: /u\n[b]: /v\n\npara\n", kinds, sizeof(kinds) / sizeof(kinds[0]));

    doc = parse_ok(quoted);
    NAOS_REQUIRE_NOT_NULL(doc);
    quote = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(quote);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_BLOCK_QUOTE, (int)quote->kind);
    NAOS_REQUIRE_UINT_EQ(2, child_count(quote));
    check_def(quoted, quote->first_child, "a", "/u", false, NULL, 0);
    check_def(quoted, quote->last_child, "b", "/v", false, NULL, 0);
    /* リンク先が次の行にある定義は 2 行。行のプレフィックスは `> ` */
    NAOS_REQUIRE_UINT_EQ(2, quote->last_child->line_count);
    NAOS_CHECK_UINT_EQ(19, quote->last_child->lines[1].prefix.start);
    NAOS_CHECK_UINT_EQ(21, quote->last_child->lines[1].content.start);
    NAOS_CHECK_UINT_EQ(3, quote->last_child->start_line);
    NAOS_CHECK_UINT_EQ(4, quote->last_child->end_line);
    NAOS_CHECK_UINT_EQ(sizeof(quoted) - 2, quote->span.end);
    mnd_md_doc_free(doc);

    doc = parse_ok("- [a]: /u\n- b\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    item = mnd_md_doc_root(doc)->first_child->first_child;
    NAOS_REQUIRE_NOT_NULL(item);
    NAOS_REQUIRE_UINT_EQ(1, child_count(item));
    NAOS_CHECK_INT_EQ((int)MND_MD_LINK_REF_DEF, (int)item->first_child->kind);
    NAOS_CHECK_UINT_EQ(9, item->span.end);
    mnd_md_doc_free(doc);
}

/* 2.8: Setext 見出しに変わる段落でも、先に定義を取り出す。行が残らなければ見出しにならない */
NAOS_TEST(block_link_ref_def_before_setext_heading)
{
    static const char data[] = "[a]: /u\nHead\n===\n";
    static const mnd_md_kind_t para_kinds[] = { MND_MD_LINK_REF_DEF, MND_MD_PARAGRAPH };
    static const mnd_md_kind_t break_kinds[] = { MND_MD_LINK_REF_DEF, MND_MD_THEMATIC_BREAK };
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *heading = NULL;
    const mnd_md_node_t *quote = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(2, child_count(root));
    check_def(data, root->first_child, "a", "/u", false, NULL, 0);
    heading = root->last_child;
    NAOS_REQUIRE_INT_EQ((int)MND_MD_HEADING, (int)heading->kind);
    NAOS_CHECK_TRUE(heading->as.heading.setext);
    NAOS_CHECK_INT_EQ(1, heading->as.heading.level);
    check_span_text(data, heading->as.heading.content, "Head");
    NAOS_CHECK_UINT_EQ(8, heading->span.start);
    NAOS_CHECK_UINT_EQ(16, heading->span.end);
    NAOS_CHECK_UINT_EQ(2, heading->start_line);
    NAOS_REQUIRE_UINT_EQ(1, heading->line_count);
    NAOS_CHECK_UINT_EQ(8, heading->lines[0].content.start);
    mnd_md_doc_free(doc);

    /* 定義だけの段落の下の `===` は段落、`---` はテーマ区切り */
    check_root_kinds("[a]: /u\n===\n", para_kinds, 2);
    check_root_kinds("[a]: /u\n---\n", break_kinds, 2);

    doc = parse_ok("> [a]: /u\n> ===\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    quote = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(quote);
    NAOS_REQUIRE_UINT_EQ(2, child_count(quote));
    NAOS_CHECK_INT_EQ((int)MND_MD_LINK_REF_DEF, (int)quote->first_child->kind);
    NAOS_CHECK_INT_EQ((int)MND_MD_PARAGRAPH, (int)quote->last_child->kind);
    NAOS_CHECK_FALSE(quote->last_child->lines[0].lazy);
    mnd_md_doc_free(doc);
}

/* 2.7: 3 種類のタイトル、行をまたぐタイトル、山括弧のリンク先、行をまたぐラベル */
NAOS_TEST(block_link_ref_def_titles_and_destinations)
{
    static const char data[] =
        "[a]: /u \"t1\"\n"
        "[b]: /u 't2'\n"
        "[c]: /u (t3)\n"
        "[d]:\n  /u\n  \"multi\nline\"\n"
        "[e]: <>\n"
        "[F\noo]: /a(b)c\\)d \"q\\\"\"\n";
    mnd_md_doc_t *doc = parse_ok(data);
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *d = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_UINT_EQ(6, child_count(root));
    check_def(data, child_at(root, 0), "a", "/u", false, "t1", '"');
    check_def(data, child_at(root, 1), "b", "/u", false, "t2", '\'');
    check_def(data, child_at(root, 2), "c", "/u", false, "t3", '(');
    d = child_at(root, 3);
    check_def(data, d, "d", "/u", false, "multi\nline", '"');
    NAOS_CHECK_UINT_EQ(4, d->line_count);
    NAOS_CHECK_UINT_EQ(4, d->start_line);
    NAOS_CHECK_UINT_EQ(7, d->end_line);
    check_def(data, child_at(root, 4), "e", "", true, NULL, 0);
    check_def(data, child_at(root, 5), "F\noo", "/a(b)c\\)d", false, "q\\\"", '"');
    NAOS_CHECK_STR_EQ("f oo", child_at(root, 5)->as.link_ref_def.norm_label);
    mnd_md_doc_free(doc);
}

/* 2.7: 定義にならない形（段落のまま）と、タイトルを諦めて定義になる形 */
NAOS_TEST(block_link_ref_def_rejections)
{
    static const mnd_md_kind_t para[] = { MND_MD_PARAGRAPH };
    static const mnd_md_kind_t def_para[] = { MND_MD_LINK_REF_DEF, MND_MD_PARAGRAPH };
    static const char *const not_defs[] = {
        "[a]:\n",               /* リンク先がない */
        "[a]: /u 'x' y\n",      /* タイトルの後ろに文字 */
        "[a]: /u y\n",          /* リンク先の後ろに文字（空白のあとのタイトルでない） */
        "[]: /u\n",             /* 空のラベル */
        "[ \t]: /u\n",          /* 空白だけのラベル */
        "[a]b]: /u\n",          /* ラベルの後ろが `:` でない */
        "[a[b]: /u\n",          /* エスケープされない `[` */
        "[a]: <b\n",            /* 閉じない山括弧 */
        "[a]: <b\nc>\n",        /* 山括弧の中の改行 */
        "[a]: <b>c\n",          /* 山括弧の後ろに文字 */
        "[a]: /u(\n",           /* 釣り合わない括弧 */
        "[a] : /u\n",           /* `]` と `:` の間の空白 */
        "    [a]: /u\n",        /* インデントコード */
        "[a]: /u 'x\n\ny'\n",   /* タイトルの中の空行（段落が終わる） */
    };
    size_t i = 0;
    char label[1100];
    size_t n = 0;

    for (i = 0; i < sizeof(not_defs) / sizeof(not_defs[0]); i++)
    {
        mnd_md_doc_t *doc = parse_ok(not_defs[i]);

        NAOS_REQUIRE_NOT_NULL(doc);
        NAOS_CHECK_INT_EQ((int)((i == 12) ? MND_MD_CODE_BLOCK : MND_MD_PARAGRAPH),
                          (int)mnd_md_doc_root(doc)->first_child->kind);
        mnd_md_doc_free(doc);
    }

    /* 次の行のタイトルの後ろに文字があれば、タイトルのない定義と段落 */
    check_root_kinds("[a]: /u\n'x' y\n", def_para, 2);
    check_root_kinds("[a]: /u\n\"\"x\n", def_para, 2);
    /* 閉じないタイトルが次の行から始まる */
    check_root_kinds("[a]: /u\n'x\n", def_para, 2);

    /* ラベルは 999 文字まで（`ä` は 2 バイトでも 1 文字） */
    label[0] = '[';
    n = 1;
    for (i = 0; i < 998; i++)
    {
        label[n++] = 'x';
    }
    label[n++] = '\xC3';
    label[n++] = '\xA4';
    memcpy(label + n, "]: /u\n", 7);
    {
        static const mnd_md_kind_t def_only[] = { MND_MD_LINK_REF_DEF };

        check_root_kinds(label, def_only, 1);
    }
    memmove(label + 2, label + 1, n + 6);
    label[1] = 'y';
    check_root_kinds(label, para, 1);
}

NAOS_SUITE(block,
    NAOS_ADD_TEST(block_list_marker_kinds),
    NAOS_ADD_TEST(block_list_start_numbers),
    NAOS_ADD_TEST(block_list_split_by_marker),
    NAOS_ADD_TEST(block_list_item_interrupting_paragraph),
    NAOS_ADD_TEST(block_list_tight_and_loose),
    NAOS_ADD_TEST(block_list_nested_tightness),
    NAOS_ADD_TEST(block_containers_hold_children_in_order),
    NAOS_ADD_TEST(block_list_item_needs_content_indent),
    NAOS_ADD_TEST(block_lazy_continuation),
    NAOS_ADD_TEST(block_atx_heading_levels),
    NAOS_ADD_TEST(block_setext_headings),
    NAOS_ADD_TEST(block_thematic_breaks),
    NAOS_ADD_TEST(block_fenced_code),
    NAOS_ADD_TEST(block_indented_code),
    NAOS_ADD_TEST(block_html_block_types),
    NAOS_ADD_TEST(block_html_block_interrupting_paragraph),
    NAOS_ADD_TEST(block_leaves_interrupt_paragraph),
    NAOS_ADD_TEST(block_link_ref_defs_before_paragraph),
    NAOS_ADD_TEST(block_link_ref_defs_only_paragraph),
    NAOS_ADD_TEST(block_link_ref_def_before_setext_heading),
    NAOS_ADD_TEST(block_link_ref_def_titles_and_destinations),
    NAOS_ADD_TEST(block_link_ref_def_rejections)
)
