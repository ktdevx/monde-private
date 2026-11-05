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

NAOS_SUITE(lines,
    NAOS_ADD_TEST(lines_paragraph_in_item_in_block_quote),
    NAOS_ADD_TEST(lines_paragraph_extra_indent_in_item),
    NAOS_ADD_TEST(lines_lazy_continuation_flagged),
    NAOS_ADD_TEST(lines_list_marker_followed_by_tab),
    NAOS_ADD_TEST(lines_block_quote_followed_by_tab)
)
