/* tree_check: 範囲の不変条件の検査そのもののテスト */

#include <naos/naos.h>

#include "tree_check.h"

#include "markdown/md.h"
#include "markdown/md_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * 正しい木の元にする文書:
 *   0: "> a\n"   引用の段落の 1 行目。prefix [0, 2)、content [2, 3)、eol 1（1 行目）
 *   4: "> b\n"   2 行目。prefix [4, 6)、content [6, 7)、eol 1（2 行目）
 *   8: "\n"      空行（3 行目）
 *   9: "text\n"  段落。prefix [9, 9)、content [9, 13)、eol 1（4 行目）
 * 根 [0, 14)、引用 [0, 7)、段落 [2, 7)（子: テキスト [2, 3)、ソフト改行 [3, 4)、テキスト [6, 7)）、
 * 段落 [9, 13)（子: テキスト [9, 13)）
 */
static const char BASE_DATA[] = "> a\n> b\n\ntext\n";
#define BASE_LEN (sizeof(BASE_DATA) - 1)

typedef struct base_tree
{
    mnd_md_doc_t *doc;
    mnd_md_node_t *quote;
    mnd_md_node_t *para1;
    mnd_md_node_t *text_a;
    mnd_md_node_t *soft;
    mnd_md_node_t *text_b;
    mnd_md_node_t *para2;
    mnd_md_node_t *text2;
    mnd_md_line_t *para1_lines;
    mnd_md_line_t *para2_lines;
} base_tree_t;

static mnd_span_t span_of(size_t start, size_t end)
{
    mnd_span_t span;

    span.start = start;
    span.end = end;

    return span;
}

static mnd_md_line_t line_of(size_t line, size_t prefix_start, size_t content_start, size_t content_end,
                             unsigned char eol_len)
{
    mnd_md_line_t result;

    memset(&result, 0, sizeof(result));
    result.line = line;
    result.prefix = span_of(prefix_start, content_start);
    result.content = span_of(content_start, content_end);
    result.eol_len = eol_len;

    return result;
}

/* 文書のアリーナにノードを作り、行番号を入れて parent の最後の子にする */
static mnd_md_node_t *add_node(mnd_md_doc_t *doc, mnd_md_node_t *parent, mnd_md_kind_t kind,
                               size_t start, size_t end, size_t start_line, size_t end_line)
{
    mnd_md_node_t *node = mnd_md_node_new(doc, kind, span_of(start, end));

    NAOS_REQUIRE_NOT_NULL(node);
    node->start_line = start_line;
    node->end_line = end_line;
    mnd_md_node_append_child(parent, node);

    return node;
}

/* 行の配列を文書のアリーナに複製してノードに付ける。変更できるように、複製へのポインタを返す */
static mnd_md_line_t *set_lines(mnd_md_doc_t *doc, mnd_md_node_t *node, const mnd_md_line_t *lines,
                                size_t count)
{
    mnd_md_line_t *copy = mnd_md_arena_dup(&doc->arena, lines, count * sizeof(lines[0]));

    NAOS_REQUIRE_NOT_NULL(copy);
    node->lines = copy;
    node->line_count = count;

    return copy;
}

static void build_base(base_tree_t *tree)
{
    mnd_md_line_t lines1[2];
    mnd_md_line_t lines2[1];
    mnd_md_node_t *root;

    memset(tree, 0, sizeof(*tree));
    tree->doc = mnd_md_doc_create(BASE_DATA, BASE_LEN, NULL);
    NAOS_REQUIRE_NOT_NULL(tree->doc);
    root = tree->doc->root;
    root->start_line = 1;
    root->end_line = 4;

    tree->quote = add_node(tree->doc, root, MND_MD_BLOCK_QUOTE, 0, 7, 1, 2);
    tree->para1 = add_node(tree->doc, tree->quote, MND_MD_PARAGRAPH, 2, 7, 1, 2);
    lines1[0] = line_of(1, 0, 2, 3, 1);
    lines1[1] = line_of(2, 4, 6, 7, 1);
    tree->para1_lines = set_lines(tree->doc, tree->para1, lines1, 2);
    tree->text_a = add_node(tree->doc, tree->para1, MND_MD_TEXT, 2, 3, 1, 1);
    tree->soft = add_node(tree->doc, tree->para1, MND_MD_SOFT_BREAK, 3, 4, 1, 1);
    tree->text_b = add_node(tree->doc, tree->para1, MND_MD_TEXT, 6, 7, 2, 2);

    tree->para2 = add_node(tree->doc, root, MND_MD_PARAGRAPH, 9, 13, 4, 4);
    lines2[0] = line_of(4, 9, 9, 13, 1);
    tree->para2_lines = set_lines(tree->doc, tree->para2, lines2, 1);
    tree->text2 = add_node(tree->doc, tree->para2, MND_MD_TEXT, 9, 13, 4, 4);
}

/* 検査が失敗し、違反したノードと行が期待どおりであることを確かめる */
static void expect_violation(const mnd_md_doc_t *doc, const mnd_md_node_t *node, size_t line_index)
{
    mnd_md_tree_check_result_t result;

    NAOS_CHECK_FALSE(mnd_md_tree_check(doc, &result));
    NAOS_CHECK_FALSE(result.ok);
    NAOS_CHECK_PTR_EQ(result.node, node);
    NAOS_CHECK_UINT_EQ(result.line_index, line_index);
    NAOS_CHECK_TRUE(result.message[0] != '\0');
    NAOS_NOTE("expected violation: %s", result.message);
}

NAOS_TEST(correct_tree_passes)
{
    base_tree_t tree;
    mnd_md_tree_check_result_t result;

    build_base(&tree);
    NAOS_CHECK_TRUE(mnd_md_tree_check(tree.doc, &result));
    NAOS_CHECK_TRUE(result.ok);
    NAOS_CHECK_NULL(result.node);
    NAOS_CHECK_UINT_EQ(result.line_index, MND_MD_TREE_CHECK_NO_LINE);
    NAOS_CHECK_STR_EQ(result.message, "");
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(tree.doc));
    NAOS_CHECK_TRUE(mnd_md_tree_check(tree.doc, NULL));
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(null_doc_fails)
{
    mnd_md_tree_check_result_t result;

    NAOS_CHECK_FALSE(mnd_md_tree_check(NULL, &result));
    NAOS_CHECK_FALSE(result.ok);
    NAOS_CHECK_NULL(result.node);
    NAOS_CHECK_TRUE(result.message[0] != '\0');
}

NAOS_TEST(empty_doc_passes)
{
    mnd_md_doc_t *doc = mnd_md_doc_create(NULL, 0, NULL);

    NAOS_REQUIRE_NOT_NULL(doc);
    doc->root->start_line = 1;
    doc->root->end_line = 1;
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
    mnd_md_doc_free(doc);
}

NAOS_TEST(child_outside_parent_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.text2->span = span_of(9, 14);
    expect_violation(tree.doc, tree.text2, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(child_before_parent_start_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.text_a->span = span_of(1, 3);
    expect_violation(tree.doc, tree.text_a, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(span_outside_content_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.para2->span = span_of(9, BASE_LEN + 1);
    expect_violation(tree.doc, tree.para2, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(reversed_span_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.soft->span = span_of(4, 3);
    expect_violation(tree.doc, tree.soft, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(overlapping_siblings_are_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.text_b->span = span_of(3, 7);
    tree.text_b->start_line = 1;
    expect_violation(tree.doc, tree.text_b, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(siblings_out_of_order_are_detected)
{
    base_tree_t tree;

    build_base(&tree);
    /* 段落を引用の前に移す。引用 [0, 7) が段落 [9, 13) の後ろに来る */
    mnd_md_node_insert_before(tree.quote, tree.para2);
    expect_violation(tree.doc, tree.quote, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(empty_siblings_at_same_offset_pass)
{
    base_tree_t tree;
    mnd_md_node_t *empty;

    build_base(&tree);
    /* ソフト改行の直前に空の範囲のノードを 2 つ置く */
    empty = mnd_md_node_new(tree.doc, MND_MD_TEXT, span_of(3, 3));
    NAOS_REQUIRE_NOT_NULL(empty);
    empty->start_line = 1;
    empty->end_line = 1;
    mnd_md_node_insert_before(tree.soft, empty);
    empty = mnd_md_node_new(tree.doc, MND_MD_TEXT, span_of(3, 3));
    NAOS_REQUIRE_NOT_NULL(empty);
    empty->start_line = 1;
    empty->end_line = 1;
    mnd_md_node_insert_before(tree.soft, empty);
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(tree.doc));
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(broken_links_are_detected)
{
    base_tree_t tree;

    build_base(&tree);
    /* soft->next は text_b だが text_b->prev が text_a を指す。前順で先に来る soft で見つかる */
    tree.text_b->prev = tree.text_a;
    expect_violation(tree.doc, tree.soft, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(wrong_start_line_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.para2->start_line = 3;
    expect_violation(tree.doc, tree.para2, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(wrong_end_line_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    /* ソフト改行 [3, 4) の最後のバイトは 1 行目の LF なので、終了行は 1（2 ではない） */
    tree.soft->end_line = 2;
    expect_violation(tree.doc, tree.soft, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(wrong_root_line_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.doc->root->end_line = 5;
    expect_violation(tree.doc, tree.doc->root, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(empty_span_line_is_its_start_line)
{
    base_tree_t tree;
    mnd_md_node_t *empty;

    build_base(&tree);
    /* 空の範囲 [9, 9) の終了行は span.start の行（4 行目） */
    empty = mnd_md_node_new(tree.doc, MND_MD_TEXT, span_of(9, 9));
    NAOS_REQUIRE_NOT_NULL(empty);
    empty->start_line = 4;
    empty->end_line = 4;
    mnd_md_node_insert_before(tree.text2, empty);
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(tree.doc));
    empty->end_line = 3;
    expect_violation(tree.doc, empty, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(prefix_not_at_line_start_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.para1_lines[1].prefix = span_of(5, 6);
    expect_violation(tree.doc, tree.para1, 1);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(prefix_not_ending_at_content_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.para1_lines[0].prefix = span_of(0, 1);
    expect_violation(tree.doc, tree.para1, 0);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(content_with_line_feed_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    /* 1 行目の内容が LF を含む */
    tree.para1_lines[0].content = span_of(2, 4);
    expect_violation(tree.doc, tree.para1, 0);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(content_with_carriage_return_is_detected)
{
    static const char data[] = "a\r\nb";
    mnd_md_doc_t *doc = mnd_md_doc_create(data, sizeof(data) - 1, NULL);
    mnd_md_node_t *para;
    mnd_md_line_t lines[2];
    mnd_md_line_t *copy;

    NAOS_REQUIRE_NOT_NULL(doc);
    doc->root->start_line = 1;
    doc->root->end_line = 2;
    para = add_node(doc, doc->root, MND_MD_PARAGRAPH, 0, 4, 1, 2);
    lines[0] = line_of(1, 0, 0, 1, 2);
    lines[1] = line_of(2, 3, 3, 4, 0);
    copy = set_lines(doc, para, lines, 2);
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));

    /* 内容が CRLF の CR を含む */
    copy[0].content = span_of(0, 2);
    expect_violation(doc, para, 0);
    mnd_md_doc_free(doc);
}

NAOS_TEST(content_starting_in_line_terminator_is_detected)
{
    static const char data[] = "a\r\nb";
    mnd_md_doc_t *doc = mnd_md_doc_create(data, sizeof(data) - 1, NULL);
    mnd_md_node_t *para;
    mnd_md_line_t lines[2];
    mnd_md_line_t *copy;

    NAOS_REQUIRE_NOT_NULL(doc);
    doc->root->start_line = 1;
    doc->root->end_line = 2;
    para = add_node(doc, doc->root, MND_MD_PARAGRAPH, 0, 4, 1, 2);
    lines[0] = line_of(1, 0, 0, 1, 2);
    lines[1] = line_of(2, 3, 3, 4, 0);
    copy = set_lines(doc, para, lines, 2);

    /* 空の内容が CRLF の CR と LF の間（物理的な行の内容の外）にある */
    copy[0].prefix = span_of(0, 2);
    copy[0].content = span_of(2, 2);
    expect_violation(doc, para, 0);
    mnd_md_doc_free(doc);
}

NAOS_TEST(lines_out_of_order_are_detected)
{
    base_tree_t tree;
    mnd_md_line_t first;

    build_base(&tree);
    first = tree.para1_lines[0];
    tree.para1_lines[0] = tree.para1_lines[1];
    tree.para1_lines[1] = first;
    expect_violation(tree.doc, tree.para1, 1);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(two_lines_on_same_physical_line_are_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.para1_lines[1] = line_of(1, 0, 2, 3, 1);
    expect_violation(tree.doc, tree.para1, 1);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(content_outside_node_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    /* 段落の範囲を 1 バイト後ろから始める。行の内容 [9, 13) が段落の外に出る */
    tree.para2->span = span_of(10, 13);
    tree.text2->span = span_of(10, 13);
    expect_violation(tree.doc, tree.para2, 0);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(wrong_line_number_of_line_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.para2_lines[0].line = 3;
    expect_violation(tree.doc, tree.para2, 0);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(wrong_eol_len_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.para1_lines[1].eol_len = 2;
    expect_violation(tree.doc, tree.para1, 1);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(pad_over_three_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.para2_lines[0].pad = 4;
    expect_violation(tree.doc, tree.para2, 0);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(lines_pointer_missing_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.para2->lines = NULL;
    expect_violation(tree.doc, tree.para2, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

NAOS_TEST(atx_heading_content_before_closing_sequence_passes)
{
    /* "# h #\n": 見出し [0, 5)、行の内容は [2, 3)（閉じの # の前で終わる） */
    static const char data[] = "# h #\n";
    mnd_md_doc_t *doc = mnd_md_doc_create(data, sizeof(data) - 1, NULL);
    mnd_md_node_t *heading;
    mnd_md_line_t line;

    NAOS_REQUIRE_NOT_NULL(doc);
    doc->root->start_line = 1;
    doc->root->end_line = 1;
    heading = add_node(doc, doc->root, MND_MD_HEADING, 0, 5, 1, 1);
    line = line_of(1, 0, 2, 3, 1);
    (void)set_lines(doc, heading, &line, 1);
    (void)add_node(doc, heading, MND_MD_TEXT, 2, 3, 1, 1);
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
    mnd_md_doc_free(doc);
}

NAOS_TEST(lone_cr_lines_share_line_number)
{
    /* 単独の CR は物理的な行を分けるが、行番号は増えない: "a\rb\n" の 2 行はどちらも 1 行目 */
    static const char data[] = "a\rb\n";
    mnd_md_doc_t *doc = mnd_md_doc_create(data, sizeof(data) - 1, NULL);
    mnd_md_node_t *para;
    mnd_md_line_t lines[2];
    mnd_md_line_t *copy;

    NAOS_REQUIRE_NOT_NULL(doc);
    doc->root->start_line = 1;
    doc->root->end_line = 1;
    para = add_node(doc, doc->root, MND_MD_PARAGRAPH, 0, 3, 1, 1);
    lines[0] = line_of(1, 0, 0, 1, 1);
    lines[1] = line_of(1, 2, 2, 3, 1);
    copy = set_lines(doc, para, lines, 2);
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));

    copy[1].line = 2;
    expect_violation(doc, para, 1);
    mnd_md_doc_free(doc);
}

NAOS_TEST(body_start_is_a_line_start)
{
    /* 本文の開始位置 4 を根の範囲の開始にする。物理的な行はそこから数え、行番号は内容全体の先頭が基準 */
    static const char data[] = "---\nfoo\n";
    mnd_md_doc_t *doc = mnd_md_doc_create(data, sizeof(data) - 1, NULL);
    mnd_md_node_t *para;
    mnd_md_line_t line;

    NAOS_REQUIRE_NOT_NULL(doc);
    doc->root->span = span_of(4, sizeof(data) - 1);
    doc->root->start_line = 2;
    doc->root->end_line = 2;
    para = add_node(doc, doc->root, MND_MD_PARAGRAPH, 4, 7, 2, 2);
    line = line_of(2, 4, 4, 7, 1);
    (void)set_lines(doc, para, &line, 1);
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));

    /* 本文の開始位置より前のノードは根の外 */
    para->span = span_of(0, 7);
    para->start_line = 1;
    expect_violation(doc, para, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(doc);
}

NAOS_TEST(root_not_reaching_end_is_detected)
{
    base_tree_t tree;

    build_base(&tree);
    tree.doc->root->span = span_of(0, 13);
    expect_violation(tree.doc, tree.doc->root, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(tree.doc);
}

#define DEEP_CHAIN_DEPTH 100000

NAOS_TEST(deep_chain_is_checked_without_recursion)
{
    static const char data[] = "a";
    mnd_md_doc_t *doc = mnd_md_doc_create(data, 1, NULL);
    mnd_md_node_t *parent;
    mnd_md_node_t *deepest = NULL;
    size_t i;

    NAOS_REQUIRE_NOT_NULL(doc);
    doc->root->start_line = 1;
    doc->root->end_line = 1;
    parent = doc->root;
    for (i = 0; i < DEEP_CHAIN_DEPTH; ++i)
    {
        deepest = add_node(doc, parent, MND_MD_BLOCK_QUOTE, 0, 1, 1, 1);
        parent = deepest;
    }
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));

    /* 最も深いノードの違反も見つかる */
    deepest->end_line = 2;
    expect_violation(doc, deepest, MND_MD_TREE_CHECK_NO_LINE);
    mnd_md_doc_free(doc);
}

NAOS_SUITE(tree_check,
    NAOS_ADD_TEST(correct_tree_passes),
    NAOS_ADD_TEST(null_doc_fails),
    NAOS_ADD_TEST(empty_doc_passes),
    NAOS_ADD_TEST(child_outside_parent_is_detected),
    NAOS_ADD_TEST(child_before_parent_start_is_detected),
    NAOS_ADD_TEST(span_outside_content_is_detected),
    NAOS_ADD_TEST(reversed_span_is_detected),
    NAOS_ADD_TEST(overlapping_siblings_are_detected),
    NAOS_ADD_TEST(siblings_out_of_order_are_detected),
    NAOS_ADD_TEST(empty_siblings_at_same_offset_pass),
    NAOS_ADD_TEST(broken_links_are_detected),
    NAOS_ADD_TEST(wrong_start_line_is_detected),
    NAOS_ADD_TEST(wrong_end_line_is_detected),
    NAOS_ADD_TEST(wrong_root_line_is_detected),
    NAOS_ADD_TEST(empty_span_line_is_its_start_line),
    NAOS_ADD_TEST(prefix_not_at_line_start_is_detected),
    NAOS_ADD_TEST(prefix_not_ending_at_content_is_detected),
    NAOS_ADD_TEST(content_with_line_feed_is_detected),
    NAOS_ADD_TEST(content_with_carriage_return_is_detected),
    NAOS_ADD_TEST(content_starting_in_line_terminator_is_detected),
    NAOS_ADD_TEST(lines_out_of_order_are_detected),
    NAOS_ADD_TEST(two_lines_on_same_physical_line_are_detected),
    NAOS_ADD_TEST(content_outside_node_is_detected),
    NAOS_ADD_TEST(wrong_line_number_of_line_is_detected),
    NAOS_ADD_TEST(wrong_eol_len_is_detected),
    NAOS_ADD_TEST(pad_over_three_is_detected),
    NAOS_ADD_TEST(lines_pointer_missing_is_detected),
    NAOS_ADD_TEST(atx_heading_content_before_closing_sequence_passes),
    NAOS_ADD_TEST(lone_cr_lines_share_line_number),
    NAOS_ADD_TEST(body_start_is_a_line_start),
    NAOS_ADD_TEST(root_not_reaching_end_is_detected),
    NAOS_ADD_TEST(deep_chain_is_checked_without_recursion)
)
