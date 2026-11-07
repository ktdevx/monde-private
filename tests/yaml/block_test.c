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
    NAOS_ADD_TEST(block_entry_out_of_memory_cleans_up)
)
