/* スイート block: 解析の入口とブロックのコレクション（2.1、2.2、2.4〜2.10、6.2〜6.4） */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "foundation/eol.h"
#include "foundation/source.h"
#include "test_alloc.h"
#include "tree_check.h"
#include "yaml/yaml.h"
#include "yaml/yaml_front_matter.h"
#include "yaml/yaml_parse.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * text 全体の範囲
 */
static mnd_span_t whole(const char *text)
{
    mnd_span_t span;

    span.start = 0;
    span.end = strlen(text);

    return span;
}

/*
 * text の content を解析し、成功して診断が増えず、木の検査を満たすことを確かめて文書を返す
 * （呼び出し側が解放する）。失敗したら NULL
 */
static mnd_yaml_doc_t *parse_ok(const char *text, size_t len, mnd_span_t content)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_doc_t *doc = NULL;
    bool ok;

    ok = mnd_yaml_parse(text, len, content, NULL, &doc, &diags);
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
    if (!ok || (doc == NULL))
    {
        mnd_yaml_doc_free(doc);
        return NULL;
    }
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(doc));

    return doc;
}

/*
 * text の content の解析が失敗し、*out が NULL になり、診断が 1 つ（エラー、message、line、パス a.md）
 * だけ追加されることを確かめる
 */
static void check_parse_error(const char *text, size_t len, mnd_span_t content, const char *message, size_t line)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;
    mnd_yaml_doc_t *doc = (mnd_yaml_doc_t *)&diags;
    const mnd_diag_t *d = NULL;

    options.path = "a.md";
    NAOS_CHECK_FALSE(mnd_yaml_parse(text, len, content, &options, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&diags));
    if (mnd_diag_list_count(&diags) == 1)
    {
        d = mnd_diag_list_at(&diags, 0);
        NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)d->severity);
        NAOS_CHECK_STR_EQ(message, d->message);
        NAOS_CHECK_STR_EQ("a.md", d->path);
        NAOS_CHECK_UINT_EQ(line, d->line);
    }
    mnd_diag_list_free(&diags);
}

/*
 * text 全体を解析し、根が style のスカラーで、論理値が value、範囲が [start, end)、
 * 行番号が start_line と end_line であることを確かめる
 */
static void check_root_scalar(const char *text, mnd_yaml_style_t style, const char *value,
                              size_t start, size_t end, size_t start_line, size_t end_line)
{
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    NAOS_CHECK_TRUE(root != NULL);
    if (root != NULL)
    {
        NAOS_CHECK_TRUE(root->parent == NULL);
        NAOS_CHECK_INT_EQ((int)MND_YAML_SCALAR, (int)root->kind);
        if (root->kind == MND_YAML_SCALAR)
        {
            NAOS_CHECK_INT_EQ((int)style, (int)root->as.scalar.style);
            NAOS_CHECK_FALSE(root->as.scalar.flow);
            NAOS_CHECK_MEM_EQ(value, strlen(value), root->as.scalar.value, root->as.scalar.value_len);
        }
        NAOS_CHECK_UINT_EQ(start, root->span.start);
        NAOS_CHECK_UINT_EQ(end, root->span.end);
        NAOS_CHECK_UINT_EQ(start_line, root->start_line);
        NAOS_CHECK_UINT_EQ(end_line, root->end_line);
    }
    mnd_yaml_doc_free(doc);
}

/*
 * text の content が空のドキュメントになり、内容・範囲・行末記号のアクセサが正しいことを確かめる
 */
static void check_empty_doc(const char *text, size_t len, mnd_span_t content, mnd_eol_t eol)
{
    mnd_yaml_doc_t *doc = parse_ok(text, len, content);
    mnd_span_t got;

    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_TRUE(mnd_yaml_doc_root(doc) == NULL);
    NAOS_CHECK_TRUE(mnd_yaml_doc_data(doc) == text);
    NAOS_CHECK_UINT_EQ(len, mnd_yaml_doc_len(doc));
    got = mnd_yaml_doc_content(doc);
    NAOS_CHECK_UINT_EQ(content.start, got.start);
    NAOS_CHECK_UINT_EQ(content.end, got.end);
    NAOS_CHECK_INT_EQ((int)eol, (int)mnd_yaml_doc_eol(doc));
    mnd_yaml_doc_free(doc);
}

/* 雛形: 既定のオプションはパスも確保関数も持たない */
NAOS_TEST(block_scaffold_options_init_is_default)
{
    mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;

    NAOS_CHECK_NULL(options.path);
    NAOS_CHECK_TRUE(options.allocator == NULL);
}

/* 範囲の検証: 文書の外の範囲と、開始が終了より後ろの範囲はエラー */
NAOS_TEST(block_entry_rejects_span_out_of_range)
{
    const char *text = "a: 1\n";
    mnd_span_t span;

    span.start = 0;
    span.end = 6;
    check_parse_error(text, 5, span, "content span out of range", 0);

    span.start = 6;
    span.end = 6;
    check_parse_error(text, 5, span, "content span out of range", 0);

    span.start = 3;
    span.end = 2;
    check_parse_error(text, 5, span, "content span out of range", 0);

    span.start = 0;
    span.end = 1;
    check_parse_error(NULL, 0, span, "content span out of range", 0);
}

/* UTF-8: 範囲の中の不正なバイト列は、文書全体の先頭から数えた行番号のエラー（6.6、7.1） */
NAOS_TEST(block_entry_rejects_invalid_utf8_in_span)
{
    const char *text = "---\n# ok\n\"a\xff\"\n---\n";
    mnd_span_t span;

    span.start = 4;
    span.end = 14;
    check_parse_error(text, strlen(text), span, "invalid UTF-8 sequence", 3);
    check_parse_error("x\n\xc3", 3, whole("x\n\xc3"), "invalid UTF-8 sequence", 2);
}

/* UTF-8: 範囲の外の不正なバイト列は検証しない */
NAOS_TEST(block_entry_ignores_invalid_utf8_outside_span)
{
    const char *text = "\xff\nfoo\n\xfe";
    mnd_yaml_doc_t *doc = NULL;
    const mnd_yaml_node_t *root = NULL;
    mnd_span_t span;

    span.start = 2;
    span.end = 6;
    doc = parse_ok(text, strlen(text), span);
    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    NAOS_CHECK_TRUE(root != NULL);
    if (root != NULL)
    {
        NAOS_CHECK_UINT_EQ(2, root->span.start);
        NAOS_CHECK_UINT_EQ(5, root->span.end);
        NAOS_CHECK_UINT_EQ(2, root->start_line);
    }
    mnd_yaml_doc_free(doc);
}

/* 空の内容は空のドキュメント（2.7）。data は len == 0 なら NULL 可。改行がなければ行末記号は LF */
NAOS_TEST(block_entry_empty_content_is_empty_doc)
{
    const char *text = "---\n---\nbody\r\n";
    mnd_span_t span;

    span.start = 0;
    span.end = 0;
    check_empty_doc(NULL, 0, span, MND_EOL_LF);
    check_empty_doc("", 0, span, MND_EOL_LF);

    /* フロントマターの内容が空（---\n---\n）: 範囲は文書の途中 */
    span.start = 4;
    span.end = 4;
    check_empty_doc(text, strlen(text), span, MND_EOL_LF);
}

/* 空行とコメントだけの内容は空のドキュメント（2.7）。行末記号は範囲の中で決まる */
NAOS_TEST(block_entry_blank_and_comment_only_is_empty_doc)
{
    const char *lf = "\n   \n# c\n  # d\n\t\n";
    const char *crlf = "# c\r\n\r\n  \r\n  # d";

    check_empty_doc(lf, strlen(lf), whole(lf), MND_EOL_LF);
    check_empty_doc(crlf, strlen(crlf), whole(crlf), MND_EOL_CRLF);
}

/* ルートが 1 つのスカラー: 各スタイルの範囲・論理値・行番号（2.1） */
NAOS_TEST(block_entry_root_scalar_styles)
{
    check_root_scalar("hello", MND_YAML_PLAIN, "hello", 0, 5, 1, 1);
    check_root_scalar("hello world  \n", MND_YAML_PLAIN, "hello world", 0, 11, 1, 1);
    check_root_scalar("'it''s'\n", MND_YAML_SINGLE_QUOTED, "it's", 0, 7, 1, 1);
    check_root_scalar("\"a\\tb\"", MND_YAML_DOUBLE_QUOTED, "a\tb", 0, 6, 1, 1);
    check_root_scalar("|\n  foo\n  bar\n", MND_YAML_LITERAL, "foo\nbar\n", 0, 13, 1, 3);
    check_root_scalar(">-\n  a\n  b\n\n", MND_YAML_FOLDED, "a b", 0, 10, 1, 3);
    check_root_scalar("  foo\n  bar\n", MND_YAML_PLAIN, "foo bar", 2, 11, 1, 2);
}

/* ルートのスカラーの前後の空行とコメントは読み飛ばし、範囲は文書の先頭を基準にする（2.5、7.1） */
NAOS_TEST(block_entry_root_scalar_skips_comments_and_blank_lines)
{
    check_root_scalar("# head\n\n  \"q\" # c\n# tail\n\n", MND_YAML_DOUBLE_QUOTED, "q", 10, 13, 3, 3);
    check_root_scalar("\n|  # c\n  x\n\n# t\n", MND_YAML_LITERAL, "x\n", 1, 11, 2, 3);
}

/* ルートがフロー形式のコレクション */
NAOS_TEST(block_entry_root_flow_collection)
{
    const char *text = "# c\n[a, {k: v},\n  'b'] # t\n";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    NAOS_CHECK_TRUE(root != NULL);
    if (root != NULL)
    {
        NAOS_CHECK_TRUE(root->parent == NULL);
        NAOS_CHECK_INT_EQ((int)MND_YAML_SEQUENCE, (int)root->kind);
        NAOS_CHECK_UINT_EQ(4, root->span.start);
        NAOS_CHECK_UINT_EQ(22, root->span.end);
        NAOS_CHECK_UINT_EQ(2, root->start_line);
        NAOS_CHECK_UINT_EQ(3, root->end_line);
        if (root->kind == MND_YAML_SEQUENCE)
        {
            NAOS_CHECK_UINT_EQ(3, root->as.sequence.count);
        }
    }
    mnd_yaml_doc_free(doc);

    text = "{k: v}";
    doc = parse_ok(text, strlen(text), whole(text));
    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    NAOS_CHECK_TRUE((root != NULL) && (root->kind == MND_YAML_MAPPING));
    mnd_yaml_doc_free(doc);
}

/* 文書の行末記号: 範囲の改行で LF と CRLF を決める。改行がなければ LF */
NAOS_TEST(block_entry_doc_eol_lf_and_crlf)
{
    const char *lf = "# c\nfoo\n";
    const char *crlf = "# c\r\nfoo\r\n";
    const char *mixed = "a\r\nb\n";
    mnd_yaml_doc_t *doc = NULL;
    mnd_span_t span;

    doc = parse_ok(lf, strlen(lf), whole(lf));
    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_INT_EQ((int)MND_EOL_LF, (int)mnd_yaml_doc_eol(doc));
    mnd_yaml_doc_free(doc);

    doc = parse_ok(crlf, strlen(crlf), whole(crlf));
    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_INT_EQ((int)MND_EOL_CRLF, (int)mnd_yaml_doc_eol(doc));
    NAOS_CHECK_TRUE(mnd_yaml_doc_root(doc) != NULL);
    if (mnd_yaml_doc_root(doc) != NULL)
    {
        NAOS_CHECK_UINT_EQ(5, mnd_yaml_doc_root(doc)->span.start);
        NAOS_CHECK_UINT_EQ(8, mnd_yaml_doc_root(doc)->span.end);
    }
    mnd_yaml_doc_free(doc);

    doc = parse_ok("foo", 3, whole("foo"));
    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_INT_EQ((int)MND_EOL_LF, (int)mnd_yaml_doc_eol(doc));
    mnd_yaml_doc_free(doc);

    /* 範囲の外の改行は数えない: 範囲 "b\n" の中は LF だけ */
    span.start = 3;
    span.end = 5;
    doc = parse_ok(mixed, strlen(mixed), span);
    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_INT_EQ((int)MND_EOL_LF, (int)mnd_yaml_doc_eol(doc));
    mnd_yaml_doc_free(doc);
}

/* 解析の前後で内容のバイト列が変わらない（2.10）。成功・失敗のどちらでも */
NAOS_TEST(block_entry_does_not_modify_content)
{
    char text[] = "# c\r\n|\r\n  a\r\n\r\n";
    char copy[sizeof(text)];
    char bad[] = "\"a\r\n";
    char bad_copy[sizeof(bad)];
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_doc_t *doc = NULL;

    memcpy(copy, text, sizeof(text));
    doc = parse_ok(text, strlen(text), whole(text));
    NAOS_CHECK_TRUE(doc != NULL);
    NAOS_CHECK_MEM_EQ(copy, sizeof(copy), text, sizeof(text));
    mnd_yaml_doc_free(doc);
    NAOS_CHECK_MEM_EQ(copy, sizeof(copy), text, sizeof(text));

    memcpy(bad_copy, bad, sizeof(bad));
    NAOS_CHECK_FALSE(mnd_yaml_parse(bad, strlen(bad), whole(bad), NULL, &doc, &diags));
    NAOS_CHECK_TRUE(doc == NULL);
    NAOS_CHECK_MEM_EQ(bad_copy, sizeof(bad_copy), bad, sizeof(bad));
    mnd_diag_list_free(&diags);
}

/* 検出器の内容の範囲を解析する: オフセットと行番号は文書全体の先頭を基準にする（7.1） */
NAOS_TEST(block_entry_offsets_relative_to_document_start)
{
    const char *text = "---\n# c\n\n  'title'\n---\nbody\n";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_front_matter_t fm;
    mnd_yaml_doc_t *doc = NULL;
    const mnd_yaml_node_t *root = NULL;
    mnd_span_t got;

    NAOS_REQUIRE_TRUE(mnd_yaml_front_matter_detect(text, strlen(text), NULL, &fm, &diags));
    NAOS_REQUIRE_TRUE(fm.present);
    mnd_diag_list_free(&diags);

    doc = parse_ok(text, strlen(text), fm.content);
    NAOS_REQUIRE_TRUE(doc != NULL);
    got = mnd_yaml_doc_content(doc);
    NAOS_CHECK_UINT_EQ(fm.content.start, got.start);
    NAOS_CHECK_UINT_EQ(fm.content.end, got.end);
    root = mnd_yaml_doc_root(doc);
    NAOS_CHECK_TRUE(root != NULL);
    if (root != NULL)
    {
        NAOS_CHECK_UINT_EQ(11, root->span.start);
        NAOS_CHECK_UINT_EQ(18, root->span.end);
        NAOS_CHECK_UINT_EQ(4, root->start_line);
        NAOS_CHECK_UINT_EQ(4, root->end_line);
        NAOS_CHECK_MEM_EQ("title", 5, root->as.scalar.value, root->as.scalar.value_len);
    }
    mnd_yaml_doc_free(doc);
}

/* ルートのスカラーの後ろの余分な内容はエラー（6.2） */
NAOS_TEST(block_entry_rejects_content_after_root_scalar)
{
    check_parse_error("\"a\" b\n", 6, whole("\"a\" b\n"), "unexpected content after value", 1);
    check_parse_error("\"a\"#c\n", 6, whole("\"a\"#c\n"), "unexpected content after value", 1);
    check_parse_error("[a] [b]", 7, whole("[a] [b]"), "unexpected content after value", 1);
    check_parse_error("# c\nfoo\n\nbar\n", 13, whole("# c\nfoo\n\nbar\n"),
                      "unexpected content after value", 4);
    check_parse_error("|\n  a\nb\n", 8, whole("|\n  a\nb\n"), "unexpected content after value", 3);
}

/* 入口でのそのほかのエラー: 構文エラーの伝播、インデントのタブ、単独の CR（6.2、6.3） */
NAOS_TEST(block_entry_reports_syntax_errors)
{
    check_parse_error("# c\n  'abc\n", 11, whole("# c\n  'abc\n"), "unterminated quoted scalar", 2);
    check_parse_error("\t\"a\"\n", 5, whole("\t\"a\"\n"), "tab character in indentation", 1);
    check_parse_error("# a\rb\nfoo\n", 10, whole("# a\rb\nfoo\n"), "carriage return without line feed", 1);
    check_parse_error("'a' # x\ry\n", 10, whole("'a' # x\ry\n"), "carriage return without line feed", 1);
    check_parse_error("'a' \r\n\r", 7, whole("'a' \r\n\r"), "carriage return without line feed", 2);
    check_parse_error("[a", 2, whole("[a"), "unterminated flow collection", 1);
}

/* メモリ不足: 失敗させる確保の位置を 1 から順に変え、false・*out == NULL・確保と解放の回数が等しい（6.7） */
NAOS_TEST(block_entry_out_of_memory_cleans_up)
{
    const char *text = "---\r\n# c\r\n[a, {k: 'v'},\r\n  \"q\"]\r\n---\r\n";
    mnd_span_t span;
    size_t n;
    bool succeeded = false;

    span.start = 5;
    span.end = strlen(text) - 5;
    for (n = 1; (n < 1000) && !succeeded; n++)
    {
        mnd_yaml_test_alloc_t ta;
        mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;
        mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
        mnd_yaml_doc_t *doc = (mnd_yaml_doc_t *)&diags;
        bool ok;

        mnd_yaml_test_alloc_init(&ta, n);
        options.allocator = &ta.allocator;
        ok = mnd_yaml_parse(text, strlen(text), span, &options, &doc, &diags);
        if (ta.failures == 0)
        {
            /* 失敗させる位置が確保の回数を超えた: 成功しているはず */
            NAOS_CHECK_TRUE(ok);
            NAOS_CHECK_TRUE(doc != NULL);
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
            if (doc != NULL)
            {
                NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(doc));
                NAOS_CHECK_TRUE(mnd_yaml_doc_root(doc) != NULL);
                NAOS_CHECK_INT_EQ((int)MND_EOL_CRLF, (int)mnd_yaml_doc_eol(doc));
            }
            mnd_yaml_doc_free(doc);
            succeeded = true;
        }
        else
        {
            NAOS_CHECK_FALSE(ok);
            NAOS_CHECK_TRUE(doc == NULL);
            NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&diags));
            if (mnd_diag_list_count(&diags) == 1)
            {
                NAOS_CHECK_STR_EQ("out of memory", mnd_diag_list_at(&diags, 0)->message);
                NAOS_CHECK_UINT_EQ(0, mnd_diag_list_at(&diags, 0)->line);
            }
        }
        NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
        mnd_diag_list_free(&diags);
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_TRUE(n > 3);
}

/* ブロック形式のコレクション（2.5） */

/*
 * node が kind のコレクションで、親が parent、項目の数が count、範囲が [start, end)、行番号が
 * start_line と end_line であることを確かめる。kind と項目の数が合えば true
 */
static bool check_collection(const mnd_yaml_node_t *node, const mnd_yaml_node_t *parent, mnd_yaml_kind_t kind,
                             size_t count, size_t start, size_t end, size_t start_line, size_t end_line)
{
    size_t got = 0;

    NAOS_CHECK_TRUE(node != NULL);
    if (node == NULL)
    {
        return false;
    }
    NAOS_CHECK_TRUE(node->parent == parent);
    NAOS_CHECK_INT_EQ((int)kind, (int)node->kind);
    NAOS_CHECK_UINT_EQ(start, node->span.start);
    NAOS_CHECK_UINT_EQ(end, node->span.end);
    NAOS_CHECK_UINT_EQ(start_line, node->start_line);
    NAOS_CHECK_UINT_EQ(end_line, node->end_line);
    if (node->kind != kind)
    {
        return false;
    }
    if (kind == MND_YAML_MAPPING)
    {
        got = node->as.mapping.count;
    }
    else if (kind == MND_YAML_SEQUENCE)
    {
        got = node->as.sequence.count;
    }
    NAOS_CHECK_UINT_EQ(count, got);

    return got == count;
}

/*
 * node がスタイル style、論理値 value、原文 raw（text の範囲のバイト列）、親 parent のブロック文脈のスカラー
 * であることを確かめる
 */
static void check_block_scalar(const char *text, const mnd_yaml_node_t *node, const mnd_yaml_node_t *parent,
                               mnd_yaml_style_t style, const char *value, const char *raw)
{
    NAOS_CHECK_TRUE(node != NULL);
    if (node == NULL)
    {
        return;
    }
    NAOS_CHECK_INT_EQ((int)MND_YAML_SCALAR, (int)node->kind);
    if (node->kind != MND_YAML_SCALAR)
    {
        return;
    }
    NAOS_CHECK_TRUE(node->parent == parent);
    NAOS_CHECK_INT_EQ((int)style, (int)node->as.scalar.style);
    NAOS_CHECK_FALSE(node->as.scalar.flow);
    NAOS_CHECK_MEM_EQ(value, strlen(value), node->as.scalar.value, node->as.scalar.value_len);
    NAOS_CHECK_MEM_EQ(raw, strlen(raw), text + node->span.start, node->span.end - node->span.start);
}

/* node が offset の位置の空のプレーンスカラー（行番号 line）で、親が parent であることを確かめる（2.8） */
static void check_empty_scalar(const mnd_yaml_node_t *node, const mnd_yaml_node_t *parent, size_t offset,
                               size_t line)
{
    NAOS_CHECK_TRUE(node != NULL);
    if (node == NULL)
    {
        return;
    }
    NAOS_CHECK_INT_EQ((int)MND_YAML_SCALAR, (int)node->kind);
    if (node->kind != MND_YAML_SCALAR)
    {
        return;
    }
    NAOS_CHECK_TRUE(node->parent == parent);
    NAOS_CHECK_INT_EQ((int)MND_YAML_PLAIN, (int)node->as.scalar.style);
    NAOS_CHECK_FALSE(node->as.scalar.flow);
    NAOS_CHECK_UINT_EQ(0, node->as.scalar.value_len);
    NAOS_CHECK_STR_EQ("", node->as.scalar.value);
    NAOS_CHECK_UINT_EQ(offset, node->span.start);
    NAOS_CHECK_UINT_EQ(offset, node->span.end);
    NAOS_CHECK_UINT_EQ(line, node->start_line);
    NAOS_CHECK_UINT_EQ(line, node->end_line);
}

/* text 全体の解析が message のエラー（行番号 line）になることを確かめる */
static void check_text_error(const char *text, const char *message, size_t line)
{
    check_parse_error(text, strlen(text), whole(text), message, line);
}

/* 入れ子のマッピング: 範囲は最初のキーの先頭から最後の値の終わりまで（2.1、2.2、2.6） */
NAOS_TEST(block_nested_mappings)
{
    const char *text = "a:\n  b: 1\n  c:\n    d: x\ne: y\n";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;
    const mnd_yaml_node_t *a = NULL;
    const mnd_yaml_node_t *c = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_MAPPING, 2, 0, 28, 1, 5))
    {
        check_block_scalar(text, root->as.mapping.pairs[0].key, root, MND_YAML_PLAIN, "a", "a");
        a = root->as.mapping.pairs[0].value;
        if (check_collection(a, root, MND_YAML_MAPPING, 2, 5, 23, 2, 4))
        {
            check_block_scalar(text, a->as.mapping.pairs[0].key, a, MND_YAML_PLAIN, "b", "b");
            check_block_scalar(text, a->as.mapping.pairs[0].value, a, MND_YAML_PLAIN, "1", "1");
            check_block_scalar(text, a->as.mapping.pairs[1].key, a, MND_YAML_PLAIN, "c", "c");
            c = a->as.mapping.pairs[1].value;
            if (check_collection(c, a, MND_YAML_MAPPING, 1, 19, 23, 4, 4))
            {
                check_block_scalar(text, c->as.mapping.pairs[0].key, c, MND_YAML_PLAIN, "d", "d");
                check_block_scalar(text, c->as.mapping.pairs[0].value, c, MND_YAML_PLAIN, "x", "x");
            }
        }
        check_block_scalar(text, root->as.mapping.pairs[1].key, root, MND_YAML_PLAIN, "e", "e");
        check_block_scalar(text, root->as.mapping.pairs[1].value, root, MND_YAML_PLAIN, "y", "y");
    }
    mnd_yaml_doc_free(doc);
}

/* 入れ子のシーケンス: 範囲は最初の - から最後の項目の終わりまで（2.2） */
NAOS_TEST(block_nested_sequences)
{
    const char *text = "- a\n-\n  - b\n  - c\n- 'd'\n";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;
    const mnd_yaml_node_t *inner = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_SEQUENCE, 3, 0, 23, 1, 5))
    {
        check_block_scalar(text, root->as.sequence.items[0], root, MND_YAML_PLAIN, "a", "a");
        inner = root->as.sequence.items[1];
        if (check_collection(inner, root, MND_YAML_SEQUENCE, 2, 8, 17, 3, 4))
        {
            check_block_scalar(text, inner->as.sequence.items[0], inner, MND_YAML_PLAIN, "b", "b");
            check_block_scalar(text, inner->as.sequence.items[1], inner, MND_YAML_PLAIN, "c", "c");
        }
        check_block_scalar(text, root->as.sequence.items[2], root, MND_YAML_SINGLE_QUOTED, "d", "'d'");
    }
    mnd_yaml_doc_free(doc);

    /* 1 行の入れ子のシーケンス（- - a）と、その後ろの同じインデントの項目 */
    text = "- - a\n  - b\n- c\n";
    doc = parse_ok(text, strlen(text), whole(text));
    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_SEQUENCE, 2, 0, 15, 1, 3))
    {
        inner = root->as.sequence.items[0];
        if (check_collection(inner, root, MND_YAML_SEQUENCE, 2, 2, 11, 1, 2))
        {
            check_block_scalar(text, inner->as.sequence.items[0], inner, MND_YAML_PLAIN, "a", "a");
            check_block_scalar(text, inner->as.sequence.items[1], inner, MND_YAML_PLAIN, "b", "b");
        }
        check_block_scalar(text, root->as.sequence.items[1], root, MND_YAML_PLAIN, "c", "c");
    }
    mnd_yaml_doc_free(doc);
}

/* キーと同じインデントで始まるシーケンスはそのキーの値（key:\n- a）。シーケンスのあとはマッピングに戻る */
NAOS_TEST(block_sequence_at_key_indent_is_value)
{
    const char *text = "key:\n- a\n- b\nnext: c\n";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;
    const mnd_yaml_node_t *seq = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_MAPPING, 2, 0, 20, 1, 4))
    {
        check_block_scalar(text, root->as.mapping.pairs[0].key, root, MND_YAML_PLAIN, "key", "key");
        seq = root->as.mapping.pairs[0].value;
        if (check_collection(seq, root, MND_YAML_SEQUENCE, 2, 5, 12, 2, 3))
        {
            check_block_scalar(text, seq->as.sequence.items[0], seq, MND_YAML_PLAIN, "a", "a");
            check_block_scalar(text, seq->as.sequence.items[1], seq, MND_YAML_PLAIN, "b", "b");
        }
        check_block_scalar(text, root->as.mapping.pairs[1].key, root, MND_YAML_PLAIN, "next", "next");
        check_block_scalar(text, root->as.mapping.pairs[1].value, root, MND_YAML_PLAIN, "c", "c");
    }
    mnd_yaml_doc_free(doc);
}

/* シーケンスの項目の中で始まるマッピング（- key: value）と、その中のキーと同じインデントのシーケンス */
NAOS_TEST(block_compact_mapping_in_sequence)
{
    const char *text = "- key: value\n  other: x\n- k2:\n  - z\n";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;
    const mnd_yaml_node_t *m = NULL;
    const mnd_yaml_node_t *seq = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_SEQUENCE, 2, 0, 35, 1, 4))
    {
        m = root->as.sequence.items[0];
        if (check_collection(m, root, MND_YAML_MAPPING, 2, 2, 23, 1, 2))
        {
            check_block_scalar(text, m->as.mapping.pairs[0].key, m, MND_YAML_PLAIN, "key", "key");
            check_block_scalar(text, m->as.mapping.pairs[0].value, m, MND_YAML_PLAIN, "value", "value");
            check_block_scalar(text, m->as.mapping.pairs[1].key, m, MND_YAML_PLAIN, "other", "other");
            check_block_scalar(text, m->as.mapping.pairs[1].value, m, MND_YAML_PLAIN, "x", "x");
        }
        m = root->as.sequence.items[1];
        if (check_collection(m, root, MND_YAML_MAPPING, 1, 26, 35, 3, 4))
        {
            check_block_scalar(text, m->as.mapping.pairs[0].key, m, MND_YAML_PLAIN, "k2", "k2");
            seq = m->as.mapping.pairs[0].value;
            if (check_collection(seq, m, MND_YAML_SEQUENCE, 1, 32, 35, 4, 4))
            {
                check_block_scalar(text, seq->as.sequence.items[0], seq, MND_YAML_PLAIN, "z", "z");
            }
        }
    }
    mnd_yaml_doc_free(doc);
}

/* キーはプレーン・シングルクオート・ダブルクオートの 3 種類（2.4）。: の前の空白はキーに含めない */
NAOS_TEST(block_three_kinds_of_keys)
{
    const char *text = "plain: 1\n'sin''gle': 2\n\"dou\\tble\" : 3\nwith space  : 4\n";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_MAPPING, 4, 0, strlen(text) - 1, 1, 4))
    {
        check_block_scalar(text, root->as.mapping.pairs[0].key, root, MND_YAML_PLAIN, "plain", "plain");
        check_block_scalar(text, root->as.mapping.pairs[0].value, root, MND_YAML_PLAIN, "1", "1");
        check_block_scalar(text, root->as.mapping.pairs[1].key, root, MND_YAML_SINGLE_QUOTED, "sin'gle",
                           "'sin''gle'");
        check_block_scalar(text, root->as.mapping.pairs[1].value, root, MND_YAML_PLAIN, "2", "2");
        check_block_scalar(text, root->as.mapping.pairs[2].key, root, MND_YAML_DOUBLE_QUOTED, "dou\tble",
                           "\"dou\\tble\"");
        check_block_scalar(text, root->as.mapping.pairs[2].value, root, MND_YAML_PLAIN, "3", "3");
        check_block_scalar(text, root->as.mapping.pairs[3].key, root, MND_YAML_PLAIN, "with space",
                           "with space");
        check_block_scalar(text, root->as.mapping.pairs[3].value, root, MND_YAML_PLAIN, "4", "4");
    }
    mnd_yaml_doc_free(doc);
}

/* コメントと空行を読み飛ばし、プレーンの途中の # は値に含める（2.5） */
NAOS_TEST(block_skips_comments_and_blank_lines)
{
    const char *text = "# head\na: 1 # c\n\n  # indented\nb:   x#y  \n# mid\nc:\n  # inner\n\n  - d # e\n\n# tail\n";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;
    const mnd_yaml_node_t *seq = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    NAOS_REQUIRE_TRUE(root != NULL);
    if (check_collection(root, NULL, MND_YAML_MAPPING, 3, 7, 66, 2, 10))
    {
        check_block_scalar(text, root->as.mapping.pairs[0].value, root, MND_YAML_PLAIN, "1", "1");
        check_block_scalar(text, root->as.mapping.pairs[1].value, root, MND_YAML_PLAIN, "x#y", "x#y");
        seq = root->as.mapping.pairs[2].value;
        if (check_collection(seq, root, MND_YAML_SEQUENCE, 1, 63, 66, 10, 10))
        {
            check_block_scalar(text, seq->as.sequence.items[0], seq, MND_YAML_PLAIN, "d", "d");
        }
    }
    mnd_yaml_doc_free(doc);
}

/* key: の後ろが空なら、: の直後の空のプレーンスカラー。- の後ろが空なら - の直後（2.8） */
NAOS_TEST(block_empty_value_position)
{
    const char *text = "a:\nb: # c\nc:";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;
    const mnd_yaml_node_t *x = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_MAPPING, 3, 0, 12, 1, 3))
    {
        check_empty_scalar(root->as.mapping.pairs[0].value, root, 2, 1);
        check_empty_scalar(root->as.mapping.pairs[1].value, root, 5, 2);
        check_empty_scalar(root->as.mapping.pairs[2].value, root, 12, 3);
    }
    mnd_yaml_doc_free(doc);

    text = "- \n-\n- x\n";
    doc = parse_ok(text, strlen(text), whole(text));
    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_SEQUENCE, 3, 0, 8, 1, 3))
    {
        check_empty_scalar(root->as.sequence.items[0], root, 1, 1);
        check_empty_scalar(root->as.sequence.items[1], root, 4, 2);
        check_block_scalar(text, root->as.sequence.items[2], root, MND_YAML_PLAIN, "x", "x");
    }
    mnd_yaml_doc_free(doc);

    /* 次の空でない行が浅いか同じインデントのキーなら空の値。空行とコメントを挟んでもよい */
    text = "x:\n  y:\n\n# c\nz: 1\n";
    doc = parse_ok(text, strlen(text), whole(text));
    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_MAPPING, 2, 0, 17, 1, 5))
    {
        x = root->as.mapping.pairs[0].value;
        if (check_collection(x, root, MND_YAML_MAPPING, 1, 5, 7, 2, 2))
        {
            check_empty_scalar(x->as.mapping.pairs[0].value, x, 7, 2);
        }
        check_block_scalar(text, root->as.mapping.pairs[1].value, root, MND_YAML_PLAIN, "1", "1");
    }
    mnd_yaml_doc_free(doc);

    /* - key: の後ろが空で、次の行が浅いシーケンスの項目 */
    text = "- key:\n- b\n";
    doc = parse_ok(text, strlen(text), whole(text));
    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_SEQUENCE, 2, 0, 10, 1, 2))
    {
        x = root->as.sequence.items[0];
        if (check_collection(x, root, MND_YAML_MAPPING, 1, 2, 6, 1, 1))
        {
            check_empty_scalar(x->as.mapping.pairs[0].value, x, 6, 1);
        }
    }
    mnd_yaml_doc_free(doc);
}

/* 値はスカラー・ブロックスカラー・フロー形式の解析に委ねる */
NAOS_TEST(block_values_delegate_to_scalars_and_flow)
{
    const char *text = "a: |\n  x\nb: [1, 2]\nc: >-\n  y\n  z\nd:\n  - |+\n    w\n\n  - {k: v}\ne: \"q\" # c\n";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;
    const mnd_yaml_node_t *v = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_MAPPING, 5, 0, strlen(text) - 5, 1, 12))
    {
        check_block_scalar(text, root->as.mapping.pairs[0].value, root, MND_YAML_LITERAL, "x\n", "|\n  x");
        v = root->as.mapping.pairs[1].value;
        if (check_collection(v, root, MND_YAML_SEQUENCE, 2, 12, 18, 3, 3))
        {
            NAOS_CHECK_TRUE(v->as.sequence.items[0]->as.scalar.flow);
        }
        check_block_scalar(text, root->as.mapping.pairs[2].value, root, MND_YAML_FOLDED, "y z", ">-\n  y\n  z");
        v = root->as.mapping.pairs[3].value;
        if (check_collection(v, root, MND_YAML_SEQUENCE, 2, 38, 60, 8, 11))
        {
            check_block_scalar(text, v->as.sequence.items[0], v, MND_YAML_LITERAL, "w\n\n", "|+\n    w\n");
            NAOS_CHECK_INT_EQ((int)MND_YAML_MAPPING, (int)v->as.sequence.items[1]->kind);
        }
        check_block_scalar(text, root->as.mapping.pairs[4].value, root, MND_YAML_DOUBLE_QUOTED, "q", "\"q\"");
    }
    mnd_yaml_doc_free(doc);
}

/* 複数行のプレーンの値は、コレクションのインデントより深い行に続く */
NAOS_TEST(block_multi_line_plain_values)
{
    const char *text = "a: b\n  c\nd:\n  - e\n    f\n  - 'g\n    h'\n";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;
    const mnd_yaml_node_t *seq = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_MAPPING, 2, 0, strlen(text) - 1, 1, 7))
    {
        check_block_scalar(text, root->as.mapping.pairs[0].value, root, MND_YAML_PLAIN, "b c", "b\n  c");
        seq = root->as.mapping.pairs[1].value;
        if (check_collection(seq, root, MND_YAML_SEQUENCE, 2, 14, strlen(text) - 1, 4, 7))
        {
            check_block_scalar(text, seq->as.sequence.items[0], seq, MND_YAML_PLAIN, "e f", "e\n    f");
            check_block_scalar(text, seq->as.sequence.items[1], seq, MND_YAML_SINGLE_QUOTED, "g h",
                               "'g\n    h'");
        }
    }
    mnd_yaml_doc_free(doc);
}

/* CRLF の文書も同じ構造で、範囲に行末記号を含めない（7.2） */
NAOS_TEST(block_crlf_document)
{
    const char *text = "a:\r\n  - b\r\n  - 'c'\r\nd: e\r\n";
    mnd_yaml_doc_t *doc = parse_ok(text, strlen(text), whole(text));
    const mnd_yaml_node_t *root = NULL;
    const mnd_yaml_node_t *seq = NULL;

    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_INT_EQ((int)MND_EOL_CRLF, (int)mnd_yaml_doc_eol(doc));
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_MAPPING, 2, 0, 24, 1, 4))
    {
        seq = root->as.mapping.pairs[0].value;
        if (check_collection(seq, root, MND_YAML_SEQUENCE, 2, 6, 18, 2, 3))
        {
            check_block_scalar(text, seq->as.sequence.items[0], seq, MND_YAML_PLAIN, "b", "b");
            check_block_scalar(text, seq->as.sequence.items[1], seq, MND_YAML_SINGLE_QUOTED, "c", "'c'");
        }
        check_block_scalar(text, root->as.mapping.pairs[1].value, root, MND_YAML_PLAIN, "e", "e");
    }
    mnd_yaml_doc_free(doc);
}

/* フロントマターの内容のマッピング: オフセットと行番号は文書全体の先頭を基準にする（7.1） */
NAOS_TEST(block_front_matter_mapping)
{
    const char *text = "---\ntitle: Hello\ntags:\n  - a\n---\nbody\n";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_front_matter_t fm;
    mnd_yaml_doc_t *doc = NULL;
    const mnd_yaml_node_t *root = NULL;

    NAOS_REQUIRE_TRUE(mnd_yaml_front_matter_detect(text, strlen(text), NULL, &fm, &diags));
    NAOS_REQUIRE_TRUE(fm.present);
    mnd_diag_list_free(&diags);
    doc = parse_ok(text, strlen(text), fm.content);
    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_doc_root(doc);
    if (check_collection(root, NULL, MND_YAML_MAPPING, 2, 4, 28, 2, 4))
    {
        check_block_scalar(text, root->as.mapping.pairs[0].value, root, MND_YAML_PLAIN, "Hello", "Hello");
        NAOS_CHECK_UINT_EQ(2, root->as.mapping.pairs[0].value->start_line);
        check_collection(root->as.mapping.pairs[1].value, root, MND_YAML_SEQUENCE, 1, 25, 28, 4, 4);
    }
    mnd_yaml_doc_free(doc);
}

/* 非対応の構文: ディレクティブ、複雑なキー、アンカー、エイリアス、タグ（6.1） */
NAOS_TEST(block_rejects_unsupported_syntax)
{
    check_text_error("%YAML 1.2\na: b\n", "unsupported YAML syntax: directive", 1);
    check_text_error("a: b\n%TAG ! x\n", "unsupported YAML syntax: directive", 2);
    check_text_error("? k\n: v\n", "unsupported YAML syntax: complex mapping key", 1);
    check_text_error("a: 1\n? b\n", "unsupported YAML syntax: complex mapping key", 2);
    check_text_error("- ? x\n", "unsupported YAML syntax: complex mapping key", 1);
    check_text_error("a: &x 1\n", "unsupported YAML syntax: anchor", 1);
    check_text_error("a: 1\nb: *x\n", "unsupported YAML syntax: alias", 2);
    check_text_error("a: !!str x\n", "unsupported YAML syntax: tag", 1);
    check_text_error("&a k: v\n", "unsupported YAML syntax: anchor", 1);
    check_text_error("- !t x\n", "unsupported YAML syntax: tag", 1);
    check_text_error("a: @x\n", "reserved indicator \"@\" cannot start a scalar", 1);
    check_text_error("- `x\n", "reserved indicator \"`\" cannot start a scalar", 1);
}

/* インデントの不整合（6.2） */
NAOS_TEST(block_rejects_inconsistent_indentation)
{
    check_text_error("a:\n    b: 1\n  c: 2\n", "inconsistent indentation", 3);
    check_text_error("a: 1\n  b: 2\n", "inconsistent indentation", 2);
    check_text_error("  a: 1\nb: 2\n", "inconsistent indentation", 2);
    check_text_error("- a\n  - b\n", "inconsistent indentation", 2);
    check_text_error("- a: 1\n   b: 2\n", "inconsistent indentation", 2);
}

/* 項目として解釈できない行（6.2） */
NAOS_TEST(block_rejects_lines_that_are_not_entries)
{
    check_text_error("a: 1\nfoo\n", "expected a mapping entry or a sequence item", 2);
    check_text_error("- a\nb\n", "expected a mapping entry or a sequence item", 2);
    check_text_error("a: 1\n- b\n", "expected a mapping entry or a sequence item", 2);
    check_text_error("a:\n  - b\n  c: 1\n", "expected a mapping entry or a sequence item", 3);
    check_text_error("a: 1\n\"b\"\n", "expected a mapping entry or a sequence item", 2);
}

/* 値の後ろの余分な内容と、そのほかの構文エラーの伝播（6.2） */
NAOS_TEST(block_rejects_content_after_value)
{
    check_text_error("a: 'x' y\n", "unexpected content after value", 1);
    check_text_error("a: b: c\n", "unexpected content after value", 1);
    check_text_error("- [a] b\n", "unexpected content after value", 1);
    check_text_error("a: - b\n", "unexpected content after value", 1);
    check_text_error("a:\n  'x' y\n", "unexpected content after value", 2);
    check_text_error("a: \"x\"#c\n", "unexpected content after value", 1);
    check_text_error("a: 'x\n", "unterminated quoted scalar", 1);
    check_text_error("a:\n  b: [x\n", "unterminated flow collection", 2);
    check_text_error("a: |x\n", "invalid block scalar", 1);
}

/* インデントのタブ（6.3） */
NAOS_TEST(block_rejects_tabs_in_indentation)
{
    check_text_error("a:\n\tb: 1\n", "tab character in indentation", 2);
    check_text_error("a:\n  b: 1\n \tc: 2\n", "tab character in indentation", 3);
    check_text_error("a:\n  - b\n\t- c\n", "tab character in indentation", 3);
    check_text_error("-\t- a\n", "tab character in indentation", 1);
}

/* 単独の CR（6.2） */
NAOS_TEST(block_rejects_lone_carriage_return)
{
    check_text_error("a: 1\nb\r: 2\n", "carriage return without line feed", 2);
    check_text_error("a:\n  b: x\ry\n", "carriage return without line feed", 2);
    check_text_error("- a\n- \r\n- b\r", "carriage return without line feed", 3);
}

/* 同じマッピングの同じキーは、後のキーの行番号のエラー（6.4）。別のマッピングなら同じキーでよい */
NAOS_TEST(block_rejects_duplicate_keys)
{
    const char *ok = "x:\n  a: 1\ny:\n  a: 2\n";
    mnd_yaml_doc_t *doc = NULL;

    check_text_error("a: 1\nb: 2\n'a': 3\n", "duplicate mapping key \"a\"", 3);
    check_text_error("x:\n  k: 1\n  \"k\": 2\n", "duplicate mapping key \"k\"", 3);
    check_text_error("- a: 1\n  a: 2\n", "duplicate mapping key \"a\"", 2);

    doc = parse_ok(ok, strlen(ok), whole(ok));
    NAOS_CHECK_TRUE(doc != NULL);
    mnd_yaml_doc_free(doc);
}

/* 10,000 段のインデントのマッピングと 10,000 段の - で、異常終了せずに解析が終わる（6.9） */
NAOS_TEST(block_deep_nesting)
{
    const size_t depth = 10000;
    size_t len = 0;
    size_t pos = 0;
    size_t levels = 0;
    size_t i;
    size_t k;
    char *data = NULL;
    mnd_yaml_doc_t *doc = NULL;
    const mnd_yaml_node_t *node = NULL;

    /* i 段目は i 個の空白と a: */
    for (i = 0; i < depth; ++i)
    {
        len += i + 3;
    }
    len += depth + 2;
    data = (char *)malloc(len + 1);
    NAOS_REQUIRE_TRUE(data != NULL);
    for (i = 0; i <= depth; ++i)
    {
        for (k = 0; k < i; ++k)
        {
            data[pos++] = ' ';
        }
        if (i < depth)
        {
            data[pos++] = 'a';
            data[pos++] = ':';
        }
        else
        {
            data[pos++] = 'x';
        }
        data[pos++] = '\n';
    }
    data[pos] = '\0';
    NAOS_CHECK_UINT_EQ(len, pos);
    doc = parse_ok(data, pos, whole(data));
    NAOS_CHECK_TRUE(doc != NULL);
    node = mnd_yaml_doc_root(doc);
    while ((node != NULL) && (node->kind == MND_YAML_MAPPING) && (node->as.mapping.count == 1))
    {
        ++levels;
        node = node->as.mapping.pairs[0].value;
    }
    NAOS_CHECK_UINT_EQ(depth, levels);
    NAOS_CHECK_TRUE((node != NULL) && (node->kind == MND_YAML_SCALAR));
    mnd_yaml_doc_free(doc);
    free(data);

    /* 1 行の 10,000 段の - */
    data = (char *)malloc((depth * 2) + 2);
    NAOS_REQUIRE_TRUE(data != NULL);
    for (i = 0; i < depth; ++i)
    {
        data[(i * 2)] = '-';
        data[(i * 2) + 1] = ' ';
    }
    data[depth * 2] = 'x';
    data[(depth * 2) + 1] = '\0';
    doc = parse_ok(data, (depth * 2) + 1, whole(data));
    NAOS_CHECK_TRUE(doc != NULL);
    levels = 0;
    node = mnd_yaml_doc_root(doc);
    while ((node != NULL) && (node->kind == MND_YAML_SEQUENCE) && (node->as.sequence.count == 1))
    {
        ++levels;
        node = node->as.sequence.items[0];
    }
    NAOS_CHECK_UINT_EQ(depth, levels);
    NAOS_CHECK_TRUE((node != NULL) && (node->kind == MND_YAML_SCALAR));
    mnd_yaml_doc_free(doc);
    free(data);
}

/*
 * メモリ不足: ブロックのコレクション（作業用の配列が伸びる項目の数）を含む文書で、失敗させる確保の位置を
 * 1 から順に変え、false・*out == NULL・確保と解放の回数が等しい（6.7）
 */
NAOS_TEST(block_collections_out_of_memory_cleans_up)
{
    const char *text = "a:\n  - b: 'c'\n    d: |\n      e\n  - [f, {g: h}]\n  - 1\n  - 2\n  - 3\n  - 4\n"
                       "i: \"j\"\nk:\nl: 1\nm: 2\nn:\n- x\n- - y\n";
    size_t n;
    size_t failures = 0;
    bool succeeded = false;

    for (n = 1; (n < 5000) && !succeeded; n++)
    {
        mnd_yaml_test_alloc_t ta;
        mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;
        mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
        mnd_yaml_doc_t *doc = (mnd_yaml_doc_t *)&diags;
        bool ok;

        mnd_yaml_test_alloc_init(&ta, n);
        options.allocator = &ta.allocator;
        ok = mnd_yaml_parse(text, strlen(text), whole(text), &options, &doc, &diags);
        if (ta.failures == 0)
        {
            NAOS_CHECK_TRUE(ok);
            NAOS_CHECK_TRUE(doc != NULL);
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
            if (doc != NULL)
            {
                NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(doc));
                NAOS_CHECK_TRUE((mnd_yaml_doc_root(doc) != NULL)
                                && (mnd_yaml_doc_root(doc)->kind == MND_YAML_MAPPING)
                                && (mnd_yaml_doc_root(doc)->as.mapping.count == 6));
            }
            mnd_yaml_doc_free(doc);
            succeeded = true;
        }
        else
        {
            ++failures;
            NAOS_CHECK_FALSE(ok);
            NAOS_CHECK_TRUE(doc == NULL);
            NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&diags));
            if (mnd_diag_list_count(&diags) == 1)
            {
                NAOS_CHECK_STR_EQ("out of memory", mnd_diag_list_at(&diags, 0)->message);
                NAOS_CHECK_UINT_EQ(0, mnd_diag_list_at(&diags, 0)->line);
            }
        }
        NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
        mnd_diag_list_free(&diags);
    }
    NAOS_CHECK_TRUE(succeeded);
    NAOS_CHECK_TRUE(failures > 5);
}

NAOS_SUITE(block,
    NAOS_ADD_TEST(block_scaffold_options_init_is_default),
    NAOS_ADD_TEST(block_entry_rejects_span_out_of_range),
    NAOS_ADD_TEST(block_entry_rejects_invalid_utf8_in_span),
    NAOS_ADD_TEST(block_entry_ignores_invalid_utf8_outside_span),
    NAOS_ADD_TEST(block_entry_empty_content_is_empty_doc),
    NAOS_ADD_TEST(block_entry_blank_and_comment_only_is_empty_doc),
    NAOS_ADD_TEST(block_entry_root_scalar_styles),
    NAOS_ADD_TEST(block_entry_root_scalar_skips_comments_and_blank_lines),
    NAOS_ADD_TEST(block_entry_root_flow_collection),
    NAOS_ADD_TEST(block_entry_doc_eol_lf_and_crlf),
    NAOS_ADD_TEST(block_entry_does_not_modify_content),
    NAOS_ADD_TEST(block_entry_offsets_relative_to_document_start),
    NAOS_ADD_TEST(block_entry_rejects_content_after_root_scalar),
    NAOS_ADD_TEST(block_entry_reports_syntax_errors),
    NAOS_ADD_TEST(block_entry_out_of_memory_cleans_up),
    NAOS_ADD_TEST(block_nested_mappings),
    NAOS_ADD_TEST(block_nested_sequences),
    NAOS_ADD_TEST(block_sequence_at_key_indent_is_value),
    NAOS_ADD_TEST(block_compact_mapping_in_sequence),
    NAOS_ADD_TEST(block_three_kinds_of_keys),
    NAOS_ADD_TEST(block_skips_comments_and_blank_lines),
    NAOS_ADD_TEST(block_empty_value_position),
    NAOS_ADD_TEST(block_values_delegate_to_scalars_and_flow),
    NAOS_ADD_TEST(block_multi_line_plain_values),
    NAOS_ADD_TEST(block_crlf_document),
    NAOS_ADD_TEST(block_front_matter_mapping),
    NAOS_ADD_TEST(block_rejects_unsupported_syntax),
    NAOS_ADD_TEST(block_rejects_inconsistent_indentation),
    NAOS_ADD_TEST(block_rejects_lines_that_are_not_entries),
    NAOS_ADD_TEST(block_rejects_content_after_value),
    NAOS_ADD_TEST(block_rejects_tabs_in_indentation),
    NAOS_ADD_TEST(block_rejects_lone_carriage_return),
    NAOS_ADD_TEST(block_rejects_duplicate_keys),
    NAOS_ADD_TEST(block_deep_nesting),
    NAOS_ADD_TEST(block_collections_out_of_memory_cleans_up)
)
