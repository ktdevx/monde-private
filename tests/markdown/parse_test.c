/* Parse: 解析の入口、開始位置、空の内容、NUL（1.x）と、入口の診断（8.1、8.2、8.4） */

#include <naos/naos.h>

#include "test_alloc.h"
#include "tree_check.h"

#include "foundation/diag.h"
#include "markdown/md.h"
#include "markdown/md_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* メモリ不足のテストで、失敗させる位置を変える回数の上限（成功するまでの確保の回数より十分に大きい） */
#define OOM_MAX_STEPS 1000

/* 期待する行の内容（prefix は [prefix_start, content_start)） */
static void check_line(const mnd_md_line_t *line, size_t line_no, size_t prefix_start,
                       size_t content_start, size_t content_end, unsigned char eol_len)
{
    NAOS_CHECK_UINT_EQ(line_no, line->line);
    NAOS_CHECK_UINT_EQ(prefix_start, line->prefix.start);
    NAOS_CHECK_UINT_EQ(content_start, line->prefix.end);
    NAOS_CHECK_UINT_EQ(content_start, line->content.start);
    NAOS_CHECK_UINT_EQ(content_end, line->content.end);
    NAOS_CHECK_UINT_EQ(0, line->pad);
    NAOS_CHECK_UINT_EQ(eol_len, line->eol_len);
    NAOS_CHECK_FALSE(line->lazy);
}

static void check_node(const mnd_md_node_t *node, mnd_md_kind_t kind, size_t start, size_t end,
                       size_t start_line, size_t end_line)
{
    NAOS_CHECK_INT_EQ((int)kind, (int)node->kind);
    NAOS_CHECK_UINT_EQ(start, node->span.start);
    NAOS_CHECK_UINT_EQ(end, node->span.end);
    NAOS_CHECK_UINT_EQ(start_line, node->start_line);
    NAOS_CHECK_UINT_EQ(end_line, node->end_line);
}

/* 解析が成功し、診断が増えず（8.4）、tree_check を満たすことを確かめる。失敗なら NULL */
static mnd_md_doc_t *parse_ok(const char *data, size_t len, size_t body_start)
{
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;
    bool ok = false;

    options.body_start = body_start;
    ok = mnd_md_parse(data, len, &options, &doc, &diags);
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_CHECK_FALSE(diags.dropped);
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

/* 失敗して *out が NULL になり、1 つのエラーの診断が message・path・line で追加されることを確かめる */
static void check_parse_error(const char *data, size_t len, size_t body_start, const char *message,
                              size_t line)
{
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = (mnd_md_doc_t *)&options; /* NULL に書き換えられることを確かめる */
    const mnd_diag_t *diag = NULL;

    options.body_start = body_start;
    options.path = "doc.md";
    NAOS_CHECK_FALSE(mnd_md_parse(data, len, &options, &doc, &diags));
    NAOS_CHECK_NULL(doc);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    diag = mnd_diag_list_at(&diags, 0);
    NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)diag->severity);
    NAOS_CHECK_STR_EQ(message, diag->message);
    NAOS_REQUIRE_NOT_NULL(diag->path);
    NAOS_CHECK_STR_EQ("doc.md", diag->path);
    NAOS_CHECK_UINT_EQ(line, diag->line);
    mnd_diag_list_free(&diags);
}

/* 子を持たない文書で、根の範囲が [body_start, len) であることを確かめる */
static void check_empty_document(const char *data, size_t len, size_t body_start, size_t start_line,
                                 size_t end_line)
{
    mnd_md_doc_t *doc = parse_ok(data, len, body_start);
    const mnd_md_node_t *root = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_NOT_NULL(root);
    check_node(root, MND_MD_DOCUMENT, body_start, len, start_line, end_line);
    NAOS_CHECK_NULL(root->first_child);
    NAOS_CHECK_NULL(root->last_child);
    NAOS_CHECK_NULL(root->lines);
    NAOS_CHECK_UINT_EQ(0, root->line_count);
    mnd_md_doc_free(doc);
}

NAOS_TEST(parse_options_init_is_all_defaults)
{
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;

    NAOS_CHECK_NULL(options.dialect);
    NAOS_CHECK_UINT_EQ(options.body_start, (size_t)0);
    NAOS_CHECK_NULL(options.path);
    NAOS_CHECK_NULL(options.allocator);
}

/* 1.1: 段落 1 つの文書の木 */
NAOS_TEST(parse_single_paragraph_tree)
{
    static const char data[] = "hello\nworld\n";
    mnd_md_doc_t *doc = parse_ok(data, sizeof(data) - 1, 0);
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_PTR_EQ(data, mnd_md_doc_data(doc));
    NAOS_CHECK_UINT_EQ(sizeof(data) - 1, mnd_md_doc_len(doc));
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_NOT_NULL(root);
    check_node(root, MND_MD_DOCUMENT, 0, 12, 1, 2);
    NAOS_CHECK_NULL(root->parent);
    para = root->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_CHECK_PTR_EQ(para, root->last_child);
    NAOS_CHECK_PTR_EQ(root, para->parent);
    check_node(para, MND_MD_PARAGRAPH, 0, 11, 1, 2);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    NAOS_REQUIRE_NOT_NULL(para->lines);
    check_line(&para->lines[0], 1, 0, 0, 5, 1);
    check_line(&para->lines[1], 2, 6, 6, 11, 1);
    mnd_md_doc_free(doc);
}

/* 1.1: 空行が段落を終わらせ、行の先頭の空白はプレフィックス、末尾の空白は内容に入る */
NAOS_TEST(parse_blank_line_separates_paragraphs)
{
    /*
     * 0: "a\n"      1 行目
     * 2: "\n"       2 行目（空行）
     * 3: "  b  \n"  3 行目。prefix [3, 5)、content [5, 8)
     * 9: "c"        4 行目（行末記号なし）
     */
    static const char data[] = "a\n\n  b  \nc";
    mnd_md_doc_t *doc = parse_ok(data, sizeof(data) - 1, 0);
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *first = NULL;
    const mnd_md_node_t *second = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    check_node(root, MND_MD_DOCUMENT, 0, 10, 1, 4);
    first = root->first_child;
    NAOS_REQUIRE_NOT_NULL(first);
    second = first->next;
    NAOS_REQUIRE_NOT_NULL(second);
    NAOS_CHECK_NULL(second->next);
    NAOS_CHECK_PTR_EQ(first, second->prev);
    NAOS_CHECK_PTR_EQ(second, root->last_child);
    check_node(first, MND_MD_PARAGRAPH, 0, 1, 1, 1);
    NAOS_REQUIRE_UINT_EQ(1, first->line_count);
    check_line(&first->lines[0], 1, 0, 0, 1, 1);
    check_node(second, MND_MD_PARAGRAPH, 5, 10, 3, 4);
    NAOS_REQUIRE_UINT_EQ(2, second->line_count);
    check_line(&second->lines[0], 3, 3, 5, 8, 1);
    check_line(&second->lines[1], 4, 9, 9, 10, 0);
    mnd_md_doc_free(doc);
}

/* 1.1、1.3: CRLF と単独の CR の行（単独の CR は行番号を増やさない） */
NAOS_TEST(parse_paragraph_lines_with_crlf_and_cr)
{
    /* 0: "a\r"  1: ... 2: "b\r\n"  5: "c" */
    static const char data[] = "a\rb\r\nc";
    mnd_md_doc_t *doc = parse_ok(data, sizeof(data) - 1, 0);
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    para = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    check_node(para, MND_MD_PARAGRAPH, 0, 6, 1, 2);
    NAOS_REQUIRE_UINT_EQ(3, para->line_count);
    check_line(&para->lines[0], 1, 0, 0, 1, 1);
    check_line(&para->lines[1], 1, 2, 2, 3, 2);
    check_line(&para->lines[2], 2, 5, 5, 6, 0);
    mnd_md_doc_free(doc);
}

/* 1.2、1.3: 開始位置より前の内容を無視し、範囲と行番号は内容全体の先頭を基準にする */
NAOS_TEST(parse_body_start_ignores_preceding_content)
{
    /* 0: "# x\n"（1 行目、対象外）  4: "hello\n"（2 行目） */
    static const char data[] = "# x\nhello\n";
    mnd_md_doc_t *doc = parse_ok(data, sizeof(data) - 1, 4);
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    check_node(root, MND_MD_DOCUMENT, 4, 10, 2, 2);
    para = root->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_CHECK_NULL(para->next);
    check_node(para, MND_MD_PARAGRAPH, 4, 9, 2, 2);
    NAOS_REQUIRE_UINT_EQ(1, para->line_count);
    check_line(&para->lines[0], 2, 4, 4, 9, 1);
    mnd_md_doc_free(doc);
}

/* 1.2: 行の途中の開始位置は、その位置を最初の行の先頭として扱う */
NAOS_TEST(parse_body_start_mid_line)
{
    /* 0: "abc  def\n"（開始位置 3 から "  def"）  9: "ghi" */
    static const char data[] = "abc  def\nghi";
    mnd_md_doc_t *doc = parse_ok(data, sizeof(data) - 1, 3);
    const mnd_md_node_t *root = NULL;
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = mnd_md_doc_root(doc);
    check_node(root, MND_MD_DOCUMENT, 3, 12, 1, 2);
    para = root->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    check_node(para, MND_MD_PARAGRAPH, 5, 12, 1, 2);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    check_line(&para->lines[0], 1, 3, 5, 8, 1);
    check_line(&para->lines[1], 2, 9, 9, 12, 0);
    mnd_md_doc_free(doc);
}

/* 1.2、1.7: 開始位置が内容の長さと等しければ、空の文書になる */
NAOS_TEST(parse_body_start_at_end_is_empty_document)
{
    static const char data[] = "abc\n";

    check_empty_document(data, sizeof(data) - 1, sizeof(data) - 1, 2, 2);
}

/* 1.4: 開始位置が長さ + 1 なら、構文木を返さずに診断を返す */
NAOS_TEST(parse_body_start_beyond_length_is_error)
{
    static const char data[] = "abc";

    check_parse_error(data, 3, 4, "body start offset 4 exceeds content length 3", 0);
    check_parse_error(NULL, 0, 1, "body start offset 1 exceeds content length 0", 0);
}

/* 8.1: 不正な UTF-8 は、その位置の行番号で診断を返す（開始位置より前も検査する） */
NAOS_TEST(parse_invalid_utf8_reports_line)
{
    static const char middle[] = "a\nb\n\xff\nc\n";
    static const char before_body[] = "\xc3\nok\n";

    check_parse_error(middle, sizeof(middle) - 1, 0, "invalid UTF-8 sequence", 3);
    check_parse_error(before_body, sizeof(before_body) - 1, 2, "invalid UTF-8 sequence", 1);
}

/* 1.5: 解析の前後で、渡された内容のバイト列が変わらない */
NAOS_TEST(parse_does_not_modify_content)
{
    static const char original[] = "# x\n  para one\r\n\tline two\r\n\n\x00tail \xe3\x81\x82\r";
    char data[sizeof(original)];
    mnd_md_doc_t *doc = NULL;

    memcpy(data, original, sizeof(original));
    doc = parse_ok(data, sizeof(data) - 1, 0);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_MEM_EQ(data, sizeof(data), original, sizeof(original));
    NAOS_CHECK_PTR_EQ(data, mnd_md_doc_data(doc));
    mnd_md_doc_free(doc);
    NAOS_CHECK_MEM_EQ(data, sizeof(data), original, sizeof(original));
}

/* 1.6: NUL を含む段落は、NUL で打ち切らずに最後まで続く */
NAOS_TEST(parse_paragraph_containing_nul)
{
    /* 0: "a\0b\n"  4: "c\0" */
    static const char data[] = "a\0b\nc\0";
    mnd_md_doc_t *doc = parse_ok(data, sizeof(data) - 1, 0);
    const mnd_md_node_t *para = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    check_node(mnd_md_doc_root(doc), MND_MD_DOCUMENT, 0, 6, 1, 2);
    para = mnd_md_doc_root(doc)->first_child;
    NAOS_REQUIRE_NOT_NULL(para);
    NAOS_CHECK_NULL(para->next);
    check_node(para, MND_MD_PARAGRAPH, 0, 6, 1, 2);
    NAOS_REQUIRE_UINT_EQ(2, para->line_count);
    check_line(&para->lines[0], 1, 0, 0, 3, 1);
    check_line(&para->lines[1], 2, 4, 4, 6, 0);
    mnd_md_doc_free(doc);
}

/* 1.7: 空の内容は子を持たない文書になる（data は NULL 可、options も NULL 可） */
NAOS_TEST(parse_empty_content)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_doc_t *doc = NULL;

    check_empty_document(NULL, 0, 0, 1, 1);
    check_empty_document("", 0, 0, 1, 1);
    NAOS_REQUIRE_TRUE(mnd_md_parse(NULL, 0, NULL, &doc, &diags));
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_CHECK_NULL(mnd_md_doc_root(doc)->first_child);
    NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
    mnd_md_doc_free(doc);
    mnd_diag_list_free(&diags);
}

/* 1.7: 空行だけの内容（LF、CRLF、CR、空白とタブ）は子を持たない文書になる */
NAOS_TEST(parse_blank_lines_only)
{
    check_empty_document("\n", 1, 0, 1, 1);
    check_empty_document("\n\n\n", 3, 0, 1, 3);
    check_empty_document("\r\n\r\n", 4, 0, 1, 2);
    check_empty_document("\r\r", 2, 0, 1, 1);
    check_empty_document(" \t\n  \n\t", 7, 0, 1, 3);
    check_empty_document("   ", 3, 0, 1, 1);
}

/* 8.2: メモリ不足では false、*out が NULL、out of memory の診断、確保したものをすべて解放 */
NAOS_TEST(parse_out_of_memory_frees_everything)
{
    static const char data[] = "first paragraph\nline two\n\n  second\r\nthird\rfourth\n\n\nlast";
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
        options.path = "oom.md";
        if (mnd_md_parse(data, sizeof(data) - 1, &options, &doc, &diags))
        {
            succeeded = true;
            NAOS_CHECK_UINT_EQ(0, ta.failures);
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
            NAOS_REQUIRE_NOT_NULL(doc);
            NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc));
            mnd_md_doc_free(doc);
        }
        else
        {
            failures++;
            NAOS_CHECK_UINT_EQ(1, ta.failures);
            NAOS_CHECK_NULL(doc);
            NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
            NAOS_CHECK_STR_EQ("out of memory", mnd_diag_list_at(&diags, 0)->message);
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_at(&diags, 0)->line);
        }
        NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
        NAOS_CHECK_UINT_EQ(0, mnd_md_test_alloc_live(&ta));
        mnd_diag_list_free(&diags);
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_UINT_GT(failures, 2);
}

NAOS_SUITE(parse,
    NAOS_ADD_TEST(parse_options_init_is_all_defaults),
    NAOS_ADD_TEST(parse_single_paragraph_tree),
    NAOS_ADD_TEST(parse_blank_line_separates_paragraphs),
    NAOS_ADD_TEST(parse_paragraph_lines_with_crlf_and_cr),
    NAOS_ADD_TEST(parse_body_start_ignores_preceding_content),
    NAOS_ADD_TEST(parse_body_start_mid_line),
    NAOS_ADD_TEST(parse_body_start_at_end_is_empty_document),
    NAOS_ADD_TEST(parse_body_start_beyond_length_is_error),
    NAOS_ADD_TEST(parse_invalid_utf8_reports_line),
    NAOS_ADD_TEST(parse_does_not_modify_content),
    NAOS_ADD_TEST(parse_paragraph_containing_nul),
    NAOS_ADD_TEST(parse_empty_content),
    NAOS_ADD_TEST(parse_blank_lines_only),
    NAOS_ADD_TEST(parse_out_of_memory_frees_everything)
)
