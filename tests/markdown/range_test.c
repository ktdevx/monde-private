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

NAOS_SUITE(range,
    NAOS_ADD_TEST(range_block_quote_ending_with_marker_only_line),
    NAOS_ADD_TEST(range_block_quote_with_lazy_line),
    NAOS_ADD_TEST(range_empty_list_item),
    NAOS_ADD_TEST(range_list_item_followed_by_blank_lines),
    NAOS_ADD_TEST(range_nested_containers),
    NAOS_ADD_TEST(range_list_marker_spans)
)
