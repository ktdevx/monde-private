/* ソース範囲と行番号（3.x） */

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

static void check_node(const mnd_md_node_t *node, mnd_md_kind_t kind, size_t start, size_t end,
                       size_t start_line, size_t end_line)
{
    NAOS_REQUIRE_NOT_NULL(node);
    NAOS_CHECK_INT_EQ((int)kind, (int)node->kind);
    NAOS_CHECK_UINT_EQ(start, node->span.start);
    NAOS_CHECK_UINT_EQ(end, node->span.end);
    NAOS_CHECK_UINT_EQ(start_line, node->start_line);
    NAOS_CHECK_UINT_EQ(end_line, node->end_line);
}

/* 3.4: `>` だけの行で終わる引用は、その `>` までを範囲にする（行末の空白と後ろの空行を含めない） */
NAOS_TEST(range_block_quote_ending_with_marker_only_line)
{
    mnd_md_doc_t *doc = parse_ok("> a\n>  \n\nb\n");
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *quote = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    quote = root->first_child;
    check_node(quote, MND_MD_BLOCK_QUOTE, 0, 5, 1, 2);
    check_node(quote->first_child, MND_MD_PARAGRAPH, 2, 3, 1, 1);
    NAOS_CHECK_NULL(quote->first_child->next);
    check_node(quote->next, MND_MD_PARAGRAPH, 9, 10, 4, 4);
    mnd_md_doc_free(doc);
}

/* 3.4: 遅延継続行の段落を含む引用は、その段落の終わりまでを範囲にする */
NAOS_TEST(range_block_quote_with_lazy_line)
{
    mnd_md_doc_t *doc = parse_ok("> a\nbc\n");
    const mnd_md_node_t *quote = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    quote = mnd_md_doc_root(doc)->first_child;
    check_node(quote, MND_MD_BLOCK_QUOTE, 0, 6, 1, 2);
    check_node(quote->first_child, MND_MD_PARAGRAPH, 2, 6, 1, 2);
    mnd_md_doc_free(doc);
}

/* 3.4、3.6: 空のリスト項目はリスト記号の終わりまで（記号の後ろの空白を含めない） */
NAOS_TEST(range_empty_list_item)
{
    mnd_md_doc_t *doc = parse_ok("-\n");
    const mnd_md_node_t *list = NULL;
    const mnd_md_node_t *item = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    list = mnd_md_doc_root(doc)->first_child;
    check_node(list, MND_MD_LIST, 0, 1, 1, 1);
    item = list->first_child;
    check_node(item, MND_MD_ITEM, 0, 1, 1, 1);
    NAOS_CHECK_UINT_EQ(0, item->as.item.marker.start);
    NAOS_CHECK_UINT_EQ(1, item->as.item.marker.end);
    NAOS_CHECK_NULL(item->first_child);
    mnd_md_doc_free(doc);

    doc = parse_ok("- a\n-   \n- b\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    list = mnd_md_doc_root(doc)->first_child;
    check_node(list, MND_MD_LIST, 0, 12, 1, 3);
    NAOS_CHECK_TRUE(list->as.list.tight);
    item = list->first_child;
    check_node(item, MND_MD_ITEM, 0, 3, 1, 1);
    item = item->next;
    check_node(item, MND_MD_ITEM, 4, 5, 2, 2);
    NAOS_CHECK_NULL(item->first_child);
    item = item->next;
    check_node(item, MND_MD_ITEM, 9, 12, 3, 3);
    check_node(item->first_child, MND_MD_PARAGRAPH, 11, 12, 3, 3);
    mnd_md_doc_free(doc);
}

/* 3.4: 後ろに空行があるリスト項目とリストは、空行を範囲に含めない */
NAOS_TEST(range_list_item_followed_by_blank_lines)
{
    mnd_md_doc_t *doc = parse_ok("- a\n\n\nb\n");
    const mnd_md_node_t *list = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    list = mnd_md_doc_root(doc)->first_child;
    check_node(list, MND_MD_LIST, 0, 3, 1, 1);
    check_node(list->first_child, MND_MD_ITEM, 0, 3, 1, 1);
    check_node(list->first_child->first_child, MND_MD_PARAGRAPH, 2, 3, 1, 1);
    check_node(list->next, MND_MD_PARAGRAPH, 6, 7, 4, 4);
    mnd_md_doc_free(doc);

    doc = parse_ok("- a\n\n\n- b\n  \n");
    NAOS_REQUIRE_NOT_NULL(doc);
    list = mnd_md_doc_root(doc)->first_child;
    check_node(list, MND_MD_LIST, 0, 9, 1, 4);
    NAOS_CHECK_FALSE(list->as.list.tight);
    check_node(list->first_child, MND_MD_ITEM, 0, 3, 1, 1);
    check_node(list->last_child, MND_MD_ITEM, 6, 9, 4, 4);
    mnd_md_doc_free(doc);
}

/* 3.4: 入れ子のコンテナは、最初の行の記号から最後の子の終わりまで */
NAOS_TEST(range_nested_containers)
{
    mnd_md_doc_t *doc = parse_ok("> - a\n>   b\n");
    const mnd_md_node_t *quote = NULL;
    const mnd_md_node_t *list = NULL;
    const mnd_md_node_t *item = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    quote = mnd_md_doc_root(doc)->first_child;
    check_node(quote, MND_MD_BLOCK_QUOTE, 0, 11, 1, 2);
    list = quote->first_child;
    check_node(list, MND_MD_LIST, 2, 11, 1, 2);
    item = list->first_child;
    check_node(item, MND_MD_ITEM, 2, 11, 1, 2);
    NAOS_CHECK_UINT_EQ(2, item->as.item.marker.start);
    NAOS_CHECK_UINT_EQ(3, item->as.item.marker.end);
    NAOS_CHECK_UINT_EQ(2, item->as.item.content_indent);
    check_node(item->first_child, MND_MD_PARAGRAPH, 4, 11, 1, 2);
    mnd_md_doc_free(doc);
}

/* 3.6: リスト記号 `-`、`10.`、`3)` の範囲 */
NAOS_TEST(range_list_marker_spans)
{
    mnd_md_doc_t *doc = parse_ok("- a\n\n10. b\n\n> 3) c\n");
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *item = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    item = root->first_child->first_child;
    check_node(item, MND_MD_ITEM, 0, 3, 1, 1);
    NAOS_CHECK_UINT_EQ(0, item->as.item.marker.start);
    NAOS_CHECK_UINT_EQ(1, item->as.item.marker.end);
    item = root->first_child->next->first_child;
    check_node(item, MND_MD_ITEM, 5, 10, 3, 3);
    NAOS_CHECK_UINT_EQ(5, item->as.item.marker.start);
    NAOS_CHECK_UINT_EQ(8, item->as.item.marker.end);
    NAOS_CHECK_UINT_EQ(10UL, root->first_child->next->as.list.start);
    item = root->last_child->first_child->first_child;
    check_node(item, MND_MD_ITEM, 14, 18, 5, 5);
    NAOS_CHECK_UINT_EQ(14, item->as.item.marker.start);
    NAOS_CHECK_UINT_EQ(16, item->as.item.marker.end);
    mnd_md_doc_free(doc);
}

/* 見出しの内容の範囲を確かめる */
static void check_heading_content(const mnd_md_node_t *heading, size_t start, size_t end)
{
    NAOS_REQUIRE_NOT_NULL(heading);
    NAOS_CHECK_UINT_EQ(start, heading->as.heading.content.start);
    NAOS_CHECK_UINT_EQ(end, heading->as.heading.content.end);
}

/* 3.4、3.5: ATX 見出しは `#` から行の終わりまで。内容は閉じの並びと前後の空白を除く */
NAOS_TEST(range_atx_heading_closing_sequence)
{
    mnd_md_doc_t *doc = parse_ok(" # foo ##   \n#\n### b ###x\n");
    const mnd_md_node_t *heading = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    heading = mnd_md_doc_root(doc)->first_child;
    check_node(heading, MND_MD_HEADING, 1, 12, 1, 1);
    check_heading_content(heading, 3, 6);
    /* 空の見出しは、記号の並びの直後の空の範囲 */
    heading = heading->next;
    check_node(heading, MND_MD_HEADING, 13, 14, 2, 2);
    check_heading_content(heading, 14, 14);
    /* 後ろに空白でない文字がある `#` の並びは閉じの並びではない */
    heading = heading->next;
    check_node(heading, MND_MD_HEADING, 15, 25, 3, 3);
    check_heading_content(heading, 19, 25);
    mnd_md_doc_free(doc);
}

/* 3.5: 閉じの並びの前のエスケープされた `#` は内容に入る（spec の「ATX headings」の例） */
NAOS_TEST(range_atx_heading_escaped_hash)
{
    mnd_md_doc_t *doc = parse_ok("### foo \\###\n## foo #\\##\n# foo \\#\n");
    const mnd_md_node_t *heading = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    heading = mnd_md_doc_root(doc)->first_child;
    check_node(heading, MND_MD_HEADING, 0, 12, 1, 1);
    check_heading_content(heading, 4, 12);
    heading = heading->next;
    check_node(heading, MND_MD_HEADING, 13, 24, 2, 2);
    check_heading_content(heading, 16, 24);
    heading = heading->next;
    check_node(heading, MND_MD_HEADING, 25, 33, 3, 3);
    check_heading_content(heading, 27, 33);
    mnd_md_doc_free(doc);
}

/* 3.4、3.5: Setext 見出しは最初の行の内容から下線の行の終わりまで。内容は 2 行で、末尾の空白を除く */
NAOS_TEST(range_setext_heading_two_lines)
{
    mnd_md_doc_t *doc = parse_ok("  Foo\nbar  \n  ===  \n\nb\n---\n");
    const mnd_md_node_t *heading = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    heading = mnd_md_doc_root(doc)->first_child;
    check_node(heading, MND_MD_HEADING, 2, 19, 1, 3);
    check_heading_content(heading, 2, 9);
    NAOS_REQUIRE_UINT_EQ(2, heading->line_count);
    NAOS_CHECK_UINT_EQ(2, heading->lines[0].content.start);
    NAOS_CHECK_UINT_EQ(11, heading->lines[1].content.end);
    heading = heading->next;
    check_node(heading, MND_MD_HEADING, 21, 26, 5, 6);
    check_heading_content(heading, 21, 22);
    NAOS_CHECK_INT_EQ(2, heading->as.heading.level);
    mnd_md_doc_free(doc);
}

/* 3.4: テーマ区切りは最初の記号から行の終わりまで */
NAOS_TEST(range_thematic_break)
{
    mnd_md_doc_t *doc = parse_ok("a\n  * * *  \n");
    const mnd_md_node_t *hr = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    hr = mnd_md_doc_root(doc)->last_child;
    check_node(hr, MND_MD_THEMATIC_BREAK, 4, 11, 2, 2);
    NAOS_CHECK_UINT_EQ(0, hr->line_count);
    mnd_md_doc_free(doc);
}

/* 3.4: フェンスコードブロックは開きフェンスから閉じフェンスの行の終わりまで。閉じがなければ最後の行まで */
NAOS_TEST(range_fenced_code)
{
    mnd_md_doc_t *doc = parse_ok(" ```\na\n``` \n> ~~~\n> b\n\nc\n");
    const mnd_md_node_t *code = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    code = mnd_md_doc_root(doc)->first_child;
    check_node(code, MND_MD_CODE_BLOCK, 1, 11, 1, 3);
    NAOS_CHECK_TRUE(code->as.code_block.closed);
    NAOS_CHECK_UINT_EQ(1, code->as.code_block.fence_indent);
    code = code->next;
    check_node(code, MND_MD_BLOCK_QUOTE, 12, 21, 4, 5);
    check_node(code->first_child, MND_MD_CODE_BLOCK, 14, 21, 4, 5);
    NAOS_CHECK_FALSE(code->first_child->as.code_block.closed);
    mnd_md_doc_free(doc);

    /* 内容のない、閉じのないフェンスは開きフェンスの行だけ */
    doc = parse_ok("~~~ x \n");
    NAOS_REQUIRE_NOT_NULL(doc);
    code = mnd_md_doc_root(doc)->first_child;
    check_node(code, MND_MD_CODE_BLOCK, 0, 6, 1, 1);
    NAOS_CHECK_UINT_EQ(4, code->as.code_block.info.start);
    NAOS_CHECK_UINT_EQ(5, code->as.code_block.info.end);
    mnd_md_doc_free(doc);
}

/* 3.4: インデントコードブロックは最初の行の内容から、末尾の空行を除いた最後の行の終わりまで */
NAOS_TEST(range_indented_code_trailing_blank_lines)
{
    mnd_md_doc_t *doc = parse_ok("    a\n\n      \nb\n");
    const mnd_md_node_t *code = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    code = mnd_md_doc_root(doc)->first_child;
    check_node(code, MND_MD_CODE_BLOCK, 4, 5, 1, 1);
    NAOS_CHECK_UINT_EQ(1, code->line_count);
    check_node(code->next, MND_MD_PARAGRAPH, 14, 15, 4, 4);
    mnd_md_doc_free(doc);

    /* リスト項目の中で、後ろに空行がある */
    doc = parse_ok("-     a\n\n\nb\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    code = mnd_md_doc_root(doc)->first_child;
    check_node(code, MND_MD_LIST, 0, 7, 1, 1);
    check_node(code->first_child, MND_MD_ITEM, 0, 7, 1, 1);
    check_node(code->first_child->first_child, MND_MD_CODE_BLOCK, 6, 7, 1, 1);
    mnd_md_doc_free(doc);
}

/* 3.4: HTML ブロックは最初の行のコンテナのプレフィックスの直後から最後の行の終わりまで */
NAOS_TEST(range_html_block)
{
    mnd_md_doc_t *doc = parse_ok("  <div>\n  x\n\ny\n");
    const mnd_md_node_t *html = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    html = mnd_md_doc_root(doc)->first_child;
    check_node(html, MND_MD_HTML_BLOCK, 0, 11, 1, 2);
    check_node(html->next, MND_MD_PARAGRAPH, 13, 14, 4, 4);
    mnd_md_doc_free(doc);
}

NAOS_SUITE(range,
    NAOS_ADD_TEST(range_block_quote_ending_with_marker_only_line),
    NAOS_ADD_TEST(range_block_quote_with_lazy_line),
    NAOS_ADD_TEST(range_empty_list_item),
    NAOS_ADD_TEST(range_list_item_followed_by_blank_lines),
    NAOS_ADD_TEST(range_nested_containers),
    NAOS_ADD_TEST(range_list_marker_spans),
    NAOS_ADD_TEST(range_atx_heading_closing_sequence),
    NAOS_ADD_TEST(range_atx_heading_escaped_hash),
    NAOS_ADD_TEST(range_setext_heading_two_lines),
    NAOS_ADD_TEST(range_thematic_break),
    NAOS_ADD_TEST(range_fenced_code),
    NAOS_ADD_TEST(range_indented_code_trailing_blank_lines),
    NAOS_ADD_TEST(range_html_block)
)
