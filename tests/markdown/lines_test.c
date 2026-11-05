/* 行ごとの内容範囲（4.x） */

#include <naos/naos.h>

#include "tree_check.h"

#include "foundation/buf.h"
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

/* 期待する行の内容（prefix は [prefix_start, content_start)） */
static void check_line(const mnd_md_line_t *line, size_t line_no, size_t prefix_start,
                       size_t content_start, size_t content_end, unsigned char pad,
                       unsigned char eol_len, bool lazy)
{
    NAOS_CHECK_UINT_EQ(line_no, line->line);
    NAOS_CHECK_UINT_EQ(prefix_start, line->prefix.start);
    NAOS_CHECK_UINT_EQ(content_start, line->prefix.end);
    NAOS_CHECK_UINT_EQ(content_start, line->content.start);
    NAOS_CHECK_UINT_EQ(content_end, line->content.end);
    NAOS_CHECK_UINT_EQ(pad, line->pad);
    NAOS_CHECK_UINT_EQ(eol_len, line->eol_len);
    NAOS_CHECK_TRUE(line->lazy == lazy);
}

/* leaf の範囲の論理的な文字列が expected と一致することを確かめる */
static void check_logical_text(const mnd_md_doc_t *doc, const mnd_md_node_t *leaf, const char *expected)
{
    mnd_buf_t buf = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_md_logical_text(doc, leaf, leaf->span, &buf));
    NAOS_CHECK_MEM_EQ(buf.data, buf.len, expected, strlen(expected));
    mnd_buf_free(&buf);
}

/* 4.1、4.2: 引用の中のリスト項目の中の 3 行の段落のプレフィックスと内容 */
NAOS_TEST(lines_paragraph_in_item_in_block_quote)
{
    mnd_md_doc_t *doc = parse_ok("> - a\n>   b\n>   c\n");
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = mnd_md_doc_root(doc)->first_child->first_child->first_child->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_PARAGRAPH, (int)para->kind);
    NAOS_REQUIRE_UINT_EQ(3, para->line_count);
    check_line(&para->lines[0], 1, 0, 4, 5, 0, 1, false);
    check_line(&para->lines[1], 2, 6, 10, 11, 0, 1, false);
    check_line(&para->lines[2], 3, 12, 16, 17, 0, 1, false);
    check_logical_text(doc, para, "a\nb\nc");
    mnd_md_doc_free(doc);
}

/* 4.2: 項目の内容のインデントより多い空白は、段落が取り除く空白としてプレフィックスに入る */
NAOS_TEST(lines_paragraph_extra_indent_in_item)
{
    mnd_md_doc_t *doc = parse_ok("-  a\n     b\n");
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = mnd_md_doc_root(doc)->first_child->first_child->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_CHECK_UINT_EQ(3, para->parent->as.item.content_indent);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    check_line(&para->lines[0], 1, 0, 3, 4, 0, 1, false);
    check_line(&para->lines[1], 2, 5, 10, 11, 0, 1, false);
    mnd_md_doc_free(doc);
}

/* 4.4: 遅延継続行に印を付ける。プレフィックスは照合できたコンテナと行の先頭の空白 */
NAOS_TEST(lines_lazy_continuation_flagged)
{
    mnd_md_doc_t *doc = parse_ok("> a\n  b\n> - c\n> d\n");
    const mnd_md_node_t *quote = NULL;
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    quote = mnd_md_doc_root(doc)->first_child;
    para = quote->first_child;
    NAOS_REQUIRE_INT_EQ((int)MND_MD_PARAGRAPH, (int)para->kind);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    check_line(&para->lines[0], 1, 0, 2, 3, 0, 1, false);
    check_line(&para->lines[1], 2, 4, 6, 7, 0, 1, true);
    check_logical_text(doc, para, "a\nb");

    para = quote->first_child->next->first_child->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_PARAGRAPH, (int)para->kind);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    check_line(&para->lines[0], 3, 8, 12, 13, 0, 1, false);
    check_line(&para->lines[1], 4, 14, 16, 17, 0, 1, true);
    check_logical_text(doc, para, "c\nd");
    mnd_md_doc_free(doc);
}

/* 4.5: `-\tfoo` では、タブ全体が記号の後ろの空白としてプレフィックスに入り、補う空白は 0 */
NAOS_TEST(lines_list_marker_followed_by_tab)
{
    mnd_md_doc_t *doc = parse_ok("-\tfoo\n\tbar\n");
    const mnd_md_node_t *item = NULL;
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    item = mnd_md_doc_root(doc)->first_child->first_child;
    NAOS_REQUIRE_INT_EQ((int)MND_MD_ITEM, (int)item->kind);
    NAOS_CHECK_UINT_EQ(4, item->as.item.content_indent);
    para = item->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    check_line(&para->lines[0], 1, 0, 2, 5, 0, 1, false);
    check_line(&para->lines[1], 2, 6, 7, 10, 0, 1, false);
    check_logical_text(doc, para, "foo\nbar");
    mnd_md_doc_free(doc);
}

/* 4.5: 引用の `>` の後ろのタブは一部だけが使われる。段落は残りの空白も取り除くので補う空白は 0 */
NAOS_TEST(lines_block_quote_followed_by_tab)
{
    mnd_md_doc_t *doc = parse_ok(">\tfoo\n");
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = mnd_md_doc_root(doc)->first_child->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_REQUIRE_UINT_EQ(1, para->line_count);
    check_line(&para->lines[0], 1, 0, 2, 5, 0, 1, false);
    mnd_md_doc_free(doc);
}

/* 4.6: 開きフェンスのインデントが 2 なら、各行から 2 桁までの空白を除く。フェンスの行は行に入らない */
NAOS_TEST(lines_fenced_code_with_indented_fence)
{
    mnd_md_doc_t *doc = parse_ok("  ```\n  a\n b\nc\n    d\n  ```\n");
    const mnd_md_node_t *code = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    code = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(code);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_CODE_BLOCK, (int)code->kind);
    NAOS_CHECK_UINT_EQ(2, code->as.code_block.fence_indent);
    NAOS_REQUIRE_UINT_EQ(4, code->line_count);
    check_line(&code->lines[0], 2, 6, 8, 9, 0, 1, false);
    check_line(&code->lines[1], 3, 10, 11, 12, 0, 1, false);
    check_line(&code->lines[2], 4, 13, 13, 14, 0, 1, false);
    check_line(&code->lines[3], 5, 15, 17, 20, 0, 1, false);
    check_logical_text(doc, code, "a\nb\nc\n  d");
    mnd_md_doc_free(doc);
}

/* 4.7: インデントコードは 4 桁をプレフィックスに入れる。4 桁に満たない空行は空白をすべて除く */
NAOS_TEST(lines_indented_code_blank_lines)
{
    mnd_md_doc_t *doc = parse_ok("    a\n  \n    b\n");
    const mnd_md_node_t *code = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    code = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(code);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_CODE_BLOCK, (int)code->kind);
    NAOS_REQUIRE_UINT_EQ(3, code->line_count);
    check_line(&code->lines[0], 1, 0, 4, 5, 0, 1, false);
    check_line(&code->lines[1], 2, 6, 8, 8, 0, 1, false);
    check_line(&code->lines[2], 3, 9, 13, 14, 0, 1, false);
    check_logical_text(doc, code, "a\n\nb");
    mnd_md_doc_free(doc);

    /* 4 桁を超える空行は、4 桁を除いた残りの空白が内容になる */
    doc = parse_ok("    a\n      \n    b\n");
    NAOS_REQUIRE_NOT_NULL(doc);
    code = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(code);
    NAOS_REQUIRE_UINT_EQ(3, code->line_count);
    check_line(&code->lines[1], 2, 6, 10, 12, 0, 1, false);
    check_logical_text(doc, code, "a\n  \nb");
    mnd_md_doc_free(doc);
}

/* 4.5、4.7: コンテナがタブの一部を使ったあとのインデントコードは、残りの桁を pad に入れる */
NAOS_TEST(lines_indented_code_pad_after_tab)
{
    static const char *const inputs[] = { "-\t\tfoo\n", ">\t\tfoo\n" };
    size_t i = 0;

    for (i = 0; i < (sizeof(inputs) / sizeof(inputs[0])); i++)
    {
        mnd_md_doc_t *doc = parse_ok(inputs[i]);
        const mnd_md_node_t *code = NULL;

        NAOS_REQUIRE_NOT_NULL(doc);
        code = mnd_md_doc_root(doc)->first_child;
        while ((code != NULL) && (code->kind != MND_MD_CODE_BLOCK))
        {
            code = code->first_child;
        }
        NAOS_REQUIRE_NOT_NULL(code);
        NAOS_CHECK_FALSE(code->as.code_block.fenced);
        NAOS_REQUIRE_UINT_EQ(1, code->line_count);
        check_line(&code->lines[0], 1, 0, 3, 6, 2, 1, false);
        check_logical_text(doc, code, "  foo");
        mnd_md_doc_free(doc);
    }
}

/* 4.3: CRLF の行の eol_len は 2 で、内容に CR が入らない */
NAOS_TEST(lines_crlf_eol_len)
{
    mnd_md_doc_t *doc = parse_ok("# a\r\n```\r\nb\r\n```\r\n    c\r\n");
    const mnd_md_node_t *node = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    node = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(node);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_HEADING, (int)node->kind);
    NAOS_REQUIRE_UINT_EQ(1, node->line_count);
    check_line(&node->lines[0], 1, 0, 2, 3, 0, 2, false);
    node = node->next;
    NAOS_REQUIRE_NOT_NULL(node);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_CODE_BLOCK, (int)node->kind);
    NAOS_CHECK_UINT_EQ(5, node->span.start);
    NAOS_CHECK_UINT_EQ(16, node->span.end);
    NAOS_REQUIRE_UINT_EQ(1, node->line_count);
    check_line(&node->lines[0], 3, 10, 10, 11, 0, 2, false);
    node = node->next;
    NAOS_REQUIRE_NOT_NULL(node);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_CODE_BLOCK, (int)node->kind);
    NAOS_REQUIRE_UINT_EQ(1, node->line_count);
    check_line(&node->lines[0], 5, 18, 22, 23, 0, 2, false);
    check_logical_text(doc, node, "c");
    mnd_md_doc_free(doc);
}

/* 4.1、4.2: ATX 見出しの行の内容は見出しの内容。HTML ブロックの行はコンテナのプレフィックスの直後から */
NAOS_TEST(lines_heading_and_html_block)
{
    mnd_md_doc_t *doc = parse_ok("## foo ##\n> <div>\n> x\n");
    const mnd_md_node_t *node = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    node = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(node);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_HEADING, (int)node->kind);
    NAOS_REQUIRE_UINT_EQ(1, node->line_count);
    check_line(&node->lines[0], 1, 0, 3, 6, 0, 1, false);
    node = node->next->first_child;
    NAOS_REQUIRE_NOT_NULL(node);
    NAOS_REQUIRE_INT_EQ((int)MND_MD_HTML_BLOCK, (int)node->kind);
    NAOS_REQUIRE_UINT_EQ(2, node->line_count);
    check_line(&node->lines[0], 2, 10, 12, 17, 0, 1, false);
    check_line(&node->lines[1], 3, 18, 20, 21, 0, 1, false);
    check_logical_text(doc, node, "<div>\nx");
    mnd_md_doc_free(doc);
}

NAOS_SUITE(lines,
    NAOS_ADD_TEST(lines_paragraph_in_item_in_block_quote),
    NAOS_ADD_TEST(lines_paragraph_extra_indent_in_item),
    NAOS_ADD_TEST(lines_lazy_continuation_flagged),
    NAOS_ADD_TEST(lines_list_marker_followed_by_tab),
    NAOS_ADD_TEST(lines_block_quote_followed_by_tab),
    NAOS_ADD_TEST(lines_fenced_code_with_indented_fence),
    NAOS_ADD_TEST(lines_indented_code_blank_lines),
    NAOS_ADD_TEST(lines_indented_code_pad_after_tab),
    NAOS_ADD_TEST(lines_crlf_eol_len),
    NAOS_ADD_TEST(lines_heading_and_html_block)
)
