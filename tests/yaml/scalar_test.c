/* スイート scalar: スカラーの走査と論理値（2.9、3.2〜3.9、6.1、6.5） */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "parser_fixture.h"
#include "test_alloc.h"
#include "tree_check.h"
#include "yaml/yaml_internal.h"
#include "yaml/yaml_scalar.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * 1 回の走査の状態。fx は parser.diags が fx.diags を指すため、初期化したあとは移動しない
 */
typedef struct scalar_run
{
    mnd_yaml_fixture_t fx;
    mnd_yaml_cursor_t at;
    mnd_yaml_node_t *node;
    bool ok;
} scalar_run_t;

/*
 * text 全体を content とする作業状態を作り、offset からスカラーを 1 つ読む。
 * 作業状態を作れなければ false（r->ok も false）
 */
static bool scalar_run(scalar_run_t *r, const char *text, size_t offset, size_t parent_indent, bool flow)
{
    mnd_span_t content;

    content.start = 0;
    content.end = strlen(text);
    r->node = NULL;
    r->ok = false;
    if (!mnd_yaml_fixture_init(&r->fx, text, strlen(text), content, NULL))
    {
        return false;
    }
    r->at = mnd_yaml_fixture_cursor(&r->fx, offset);
    r->ok = mnd_yaml_scalar_parse(&r->fx.parser, &r->at, parent_indent, flow, &r->node);

    return true;
}

/*
 * text の offset から読んだスカラーのスタイル・論理値・範囲（原文 raw）を確かめ、
 * カーソルが範囲の終わりにあること、診断がないこと、木の検査を満たすことを確かめる
 */
static void check_scalar(const char *text, size_t offset, size_t parent_indent, bool flow, mnd_yaml_style_t style,
                         const char *value, size_t value_len, const char *raw)
{
    scalar_run_t r;
    size_t raw_len = strlen(raw);

    NAOS_REQUIRE_TRUE(scalar_run(&r, text, offset, parent_indent, flow));
    NAOS_CHECK_TRUE(r.ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&r.fx.diags));
    if (r.ok && (r.node != NULL))
    {
        NAOS_CHECK_INT_EQ((int)MND_YAML_SCALAR, (int)r.node->kind);
        NAOS_CHECK_INT_EQ((int)style, (int)r.node->as.scalar.style);
        NAOS_CHECK_TRUE(r.node->as.scalar.flow == flow);
        NAOS_CHECK_MEM_EQ(value, value_len, r.node->as.scalar.value, r.node->as.scalar.value_len);
        NAOS_CHECK_INT_EQ(0, (int)r.node->as.scalar.value[r.node->as.scalar.value_len]);
        NAOS_CHECK_UINT_EQ(offset, r.node->span.start);
        NAOS_CHECK_MEM_EQ(raw, raw_len, text + r.node->span.start, r.node->span.end - r.node->span.start);
        NAOS_CHECK_UINT_EQ(r.node->span.end, r.at.offset);
        NAOS_CHECK_UINT_EQ(mnd_yaml_lines_index_of(&r.fx.parser.lines, r.node->span.end), r.at.line_index);
        mnd_yaml_doc_set_root(r.fx.parser.doc, r.node);
        NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(r.fx.parser.doc));
    }
    else
    {
        NAOS_CHECK_TRUE(r.node != NULL);
    }
    mnd_yaml_fixture_cleanup(&r.fx);
}

/* check_scalar の論理値を NUL 終端の文字列で渡す版 */
static void check_value(const char *text, size_t offset, size_t parent_indent, bool flow, mnd_yaml_style_t style,
                        const char *value, const char *raw)
{
    check_scalar(text, offset, parent_indent, flow, style, value, strlen(value), raw);
}

/*
 * text の offset から読むと、message のエラーの診断が line の行で 1 つだけ追加され、
 * false が返り、*out と *at が変わらないことを確かめる
 */
static void check_error(const char *text, size_t offset, size_t parent_indent, bool flow, const char *message,
                        size_t line)
{
    scalar_run_t r;
    const mnd_diag_t *d = NULL;
    mnd_yaml_cursor_t before;

    NAOS_REQUIRE_TRUE(scalar_run(&r, text, offset, parent_indent, flow));
    before = mnd_yaml_fixture_cursor(&r.fx, offset);
    NAOS_CHECK_FALSE(r.ok);
    NAOS_CHECK_TRUE(r.node == NULL);
    NAOS_CHECK_UINT_EQ(before.offset, r.at.offset);
    NAOS_CHECK_UINT_EQ(before.line_index, r.at.line_index);
    NAOS_CHECK_TRUE(r.fx.parser.failed);
    NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&r.fx.diags));
    if (mnd_diag_list_count(&r.fx.diags) == 1)
    {
        d = mnd_diag_list_at(&r.fx.diags, 0);
        NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)d->severity);
        NAOS_CHECK_STR_EQ(message, d->message);
        NAOS_CHECK_UINT_EQ(line, d->line);
    }
    mnd_yaml_fixture_cleanup(&r.fx);
}

/* 雛形: 作業状態のエラーの報告は最初のエラーだけを診断に追加する（1 回の解析で返す診断は 1 つ） */
NAOS_TEST(scalar_scaffold_parser_reports_first_error_only)
{
    mnd_diag_list_t diags;
    mnd_yaml_parser_t p;
    const mnd_diag_t *d = NULL;

    memset(&p, 0, sizeof(p));
    mnd_diag_list_init(&diags);
    p.path = "a.md";
    p.diags = &diags;
    mnd_yaml_parser_error(&p, 3, "unterminated quoted scalar");
    NAOS_CHECK_TRUE(p.failed);
    NAOS_CHECK_FALSE(mnd_yaml_parser_claim_error(&p));
    mnd_yaml_parser_error(&p, 5, "inconsistent indentation");
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    d = mnd_diag_list_at(&diags, 0);
    NAOS_REQUIRE_TRUE(d != NULL);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_CHECK_STR_EQ("unterminated quoted scalar", d->message);
    NAOS_CHECK_STR_EQ("a.md", d->path);
    NAOS_CHECK_UINT_EQ(3, d->line);
    mnd_diag_list_free(&diags);
}

/* 作業状態の組み立て: 文書・行の表・行末記号を作り、片付けで確保と解放の回数が等しくなる */
NAOS_TEST(scalar_fixture_builds_and_cleans_up)
{
    const char *text = "a: 1\r\nb: 2\r\n";
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_fixture_t fx;
    mnd_span_t content;
    mnd_yaml_cursor_t at;

    content.start = 0;
    content.end = strlen(text);
    mnd_yaml_test_alloc_init(&ta, 0);
    NAOS_REQUIRE_TRUE(mnd_yaml_fixture_init(&fx, text, strlen(text), content, &ta.allocator));
    NAOS_CHECK_TRUE(fx.parser.doc != NULL);
    NAOS_CHECK_TRUE(fx.parser.diags == &fx.diags);
    NAOS_CHECK_FALSE(fx.parser.failed);
    NAOS_CHECK_TRUE(fx.parser.path == NULL);
    NAOS_CHECK_UINT_EQ(2, fx.parser.lines.count);
    NAOS_CHECK_INT_EQ((int)MND_EOL_CRLF, (int)mnd_yaml_doc_eol(fx.parser.doc));
    at = mnd_yaml_fixture_cursor(&fx, 9);
    NAOS_CHECK_UINT_EQ(1, at.line_index);
    NAOS_CHECK_UINT_EQ(9, at.offset);
    mnd_yaml_fixture_cleanup(&fx);
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));
    mnd_yaml_fixture_cleanup(&fx);
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));

    /* 確保の失敗では false を返し、片付けたあとに確保が残らない */
    mnd_yaml_test_alloc_init(&ta, 1);
    NAOS_CHECK_FALSE(mnd_yaml_fixture_init(&fx, text, strlen(text), content, &ta.allocator));
    mnd_yaml_fixture_cleanup(&fx);
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));
    mnd_yaml_test_alloc_init(&ta, 2);
    NAOS_CHECK_FALSE(mnd_yaml_fixture_init(&fx, text, strlen(text), content, &ta.allocator));
    mnd_yaml_fixture_cleanup(&fx);
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));
}

/* 各スタイルの範囲（引用符を含み、後ろの空白とコメントを含まない）とスタイル（3.2、3.3） */
NAOS_TEST(scalar_span_and_style_of_each_style)
{
    check_value("key: hello world  # c\n", 5, 0, false, MND_YAML_PLAIN, "hello world", "hello world");
    check_value("key: 'a b'  # c\n", 5, 0, false, MND_YAML_SINGLE_QUOTED, "a b", "'a b'");
    check_value("key: \"a b\" # c\n", 5, 0, false, MND_YAML_DOUBLE_QUOTED, "a b", "\"a b\"");
    check_value("key: ''\n", 5, 0, false, MND_YAML_SINGLE_QUOTED, "", "''");
    check_value("key: \"\"", 5, 0, false, MND_YAML_DOUBLE_QUOTED, "", "\"\"");
    check_value("v", 0, 0, false, MND_YAML_PLAIN, "v", "v");
}

/* ダブルクオートのエスケープの解決（3.5） */
NAOS_TEST(scalar_double_quoted_escapes)
{
    static const char expected[] = "a\tb\xc3\xa9" "A\\\"";

    /* 原文は "a\tbé\x41\\\""（é は UTF-8 のまま） */
    check_scalar("\"a\\tb\xc3\xa9\\x41\\\\\\\"\"", 0, 0, false, MND_YAML_DOUBLE_QUOTED, expected,
                 sizeof(expected) - 1, "\"a\\tb\xc3\xa9\\x41\\\\\\\"\"");
    check_value("\"\\a\\b\\t\\n\\v\\f\\r\\e\\ \\\"\\/\\\\\"", 0, 0, false, MND_YAML_DOUBLE_QUOTED,
                "\a\b\t\n\v\f\r\x1b \"/\\", "\"\\a\\b\\t\\n\\v\\f\\r\\e\\ \\\"\\/\\\\\"");
    check_value("\"\\N\\_\\L\\P\"", 0, 0, false, MND_YAML_DOUBLE_QUOTED,
                "\xc2\x85\xc2\xa0\xe2\x80\xa8\xe2\x80\xa9", "\"\\N\\_\\L\\P\"");
    check_value("\"\\u00e9\\u3042\\U0001F600\\x7f\\U0010FFFF\"", 0, 0, false, MND_YAML_DOUBLE_QUOTED,
                "\xc3\xa9\xe3\x81\x82\xf0\x9f\x98\x80\x7f\xf4\x8f\xbf\xbf",
                "\"\\u00e9\\u3042\\U0001F600\\x7f\\U0010FFFF\"");
}

/*
 * ダブルクオートの \ の後ろのタブ文字は U+0009（3.5。YAML 1.2 の ns-esc-horizontal-tab）。
 * 行末にあってもエスケープなので行末の空白として除かず、続く行の境目は空白に折り返す（PyYAML と同じ）
 */
NAOS_TEST(scalar_double_quoted_escaped_tab)
{
    check_value("\"a\\\tb\"", 0, 0, false, MND_YAML_DOUBLE_QUOTED, "a\tb", "\"a\\\tb\"");
    check_value("k: \"a\\\tb\" # c\n", 3, 0, false, MND_YAML_DOUBLE_QUOTED, "a\tb", "\"a\\\tb\"");
    check_value("k: \"\\\t\"\n", 3, 0, false, MND_YAML_DOUBLE_QUOTED, "\t", "\"\\\t\"");
    check_value("k: \"a\\\t\n  b\"\n", 3, 0, false, MND_YAML_DOUBLE_QUOTED, "a\t b", "\"a\\\t\n  b\"");
    check_value("k: \"a\\\t\t \n  b\"\n", 3, 0, false, MND_YAML_DOUBLE_QUOTED, "a\t b",
                "\"a\\\t\t \n  b\"");
    check_value("[\"a\\\tb\", c]", 1, 0, true, MND_YAML_DOUBLE_QUOTED, "a\tb", "\"a\\\tb\"");
}

/* シングルクオートの '' は '（3.6） */
NAOS_TEST(scalar_single_quoted_doubled_quote)
{
    check_value("k: 'it''s'\n", 3, 0, false, MND_YAML_SINGLE_QUOTED, "it's", "'it''s'");
    check_value("k: ''''\n", 3, 0, false, MND_YAML_SINGLE_QUOTED, "'", "''''");
    check_value("k: 'a\\n\"'\n", 3, 0, false, MND_YAML_SINGLE_QUOTED, "a\\n\"", "'a\\n\"'");
}

/* 複数行のプレーンの折り返し: 行の境目は空白、空行は改行、末尾の空行は含めない（3.7） */
NAOS_TEST(scalar_multi_line_plain_folding)
{
    check_value("k: a\n  b  \n\n  c d\n\n\n  e\n\nx: 1\n", 3, 0, false, MND_YAML_PLAIN, "a b\nc d\n\ne",
                "a\n  b  \n\n  c d\n\n\n  e");
    /* より深い構造の行（key: と - ）、浅い行、コメントの行は継続行にしない */
    check_value("k: a\n  b: c\n", 3, 0, false, MND_YAML_PLAIN, "a", "a");
    check_value("k: a\n  - b\n", 3, 0, false, MND_YAML_PLAIN, "a", "a");
    check_value("k: a\nb\n", 3, 0, false, MND_YAML_PLAIN, "a", "a");
    check_value("k: a\n  # c\n  d\n", 3, 0, false, MND_YAML_PLAIN, "a", "a");
    /* 継続行の後ろのコメントで終わる */
    check_value("k: a\n  b # c\n  d\n", 3, 0, false, MND_YAML_PLAIN, "a b", "a\n  b");
    /* 親のインデントより深い行だけが継続行（シーケンスの項目の中） */
    check_value("  - a\n    b\n  - c\n", 4, 2, false, MND_YAML_PLAIN, "a b", "a\n    b");
    /* 継続行の - と ? と : は後ろが空白でなければ値の一部 */
    check_value("k: a\n  -b ?c :d\n", 3, 0, false, MND_YAML_PLAIN, "a -b ?c :d", "a\n  -b ?c :d");
}

/* 複数行の引用符付きの折り返し（3.7） */
NAOS_TEST(scalar_multi_line_quoted_folding)
{
    check_value("k: 'a  \n  b\n\n  c  '\n", 3, 0, false, MND_YAML_SINGLE_QUOTED, "a b\nc  ",
                "'a  \n  b\n\n  c  '");
    check_value("k: \"  a  \n  b\n\n\n  c\"\n", 3, 0, false, MND_YAML_DOUBLE_QUOTED, "  a b\n\nc",
                "\"  a  \n  b\n\n\n  c\"");
    /* 引用符の中のコメントのような行は内容 */
    check_value("k: 'a\n  # b'\n", 3, 0, false, MND_YAML_SINGLE_QUOTED, "a # b", "'a\n  # b'");
    /* 引用符付きの継続行はインデントによらない */
    check_value("k: \"a\nb\"\n", 3, 0, false, MND_YAML_DOUBLE_QUOTED, "a b", "\"a\nb\"");
}

/* ダブルクオートの行末の \ は折り返しを抑止し、その前の空白は残す（3.7。YAML 1.2 の例 7.5） */
NAOS_TEST(scalar_double_quoted_escaped_line_break)
{
    check_value("k: \"a  \\\n   b c \\\n\n  d\"\n", 3, 0, false, MND_YAML_DOUBLE_QUOTED, "a  b c \nd",
                "\"a  \\\n   b c \\\n\n  d\"");
    check_value("k: \"a \\\r\n  b\"\r\n", 3, 0, false, MND_YAML_DOUBLE_QUOTED, "a b", "\"a \\\r\n  b\"");
    /* エスケープした空白とタブは除かない */
    check_value("k: \"a\\t\n  \\ b\"\n", 3, 0, false, MND_YAML_DOUBLE_QUOTED, "a\t  b", "\"a\\t\n  \\ b\"");
}

/* CRLF の文書でも論理値の改行は LF（3.9） */
NAOS_TEST(scalar_crlf_document_uses_lf_in_values)
{
    check_value("k: a\r\n  b\r\n\r\n  c\r\nx: 1\r\n", 3, 0, false, MND_YAML_PLAIN, "a b\nc",
                "a\r\n  b\r\n\r\n  c");
    check_value("k: 'a\r\n\r\n  b'\r\n", 3, 0, false, MND_YAML_SINGLE_QUOTED, "a\nb", "'a\r\n\r\n  b'");
    check_value("k: \"a\\\r\n  b\r\n  c\"\r\n", 3, 0, false, MND_YAML_DOUBLE_QUOTED, "ab c",
                "\"a\\\r\n  b\r\n  c\"");
}

/* 値の型は解釈せず、すべて文字列（2.9） */
NAOS_TEST(scalar_values_are_strings)
{
    check_value("k: true\n", 3, 0, false, MND_YAML_PLAIN, "true", "true");
    check_value("k: 123\n", 3, 0, false, MND_YAML_PLAIN, "123", "123");
    check_value("k: ~\n", 3, 0, false, MND_YAML_PLAIN, "~", "~");
    check_value("k: 1.5e3\n", 3, 0, false, MND_YAML_PLAIN, "1.5e3", "1.5e3");
}

/* プレーンの途中の #、&、*、!、: は値の一部（2.5） */
NAOS_TEST(scalar_plain_indicators_inside_value)
{
    check_value("k: a#b &c *d !e\n", 3, 0, false, MND_YAML_PLAIN, "a#b &c *d !e", "a#b &c *d !e");
    check_value("k: http://x.y/z # c\n", 3, 0, false, MND_YAML_PLAIN, "http://x.y/z", "http://x.y/z");
    check_value("k: -1\n", 3, 0, false, MND_YAML_PLAIN, "-1", "-1");
    /* キーの位置では : の前で終わる */
    check_value("key: v\n", 0, 0, false, MND_YAML_PLAIN, "key", "key");
    check_value("key:\n", 0, 0, false, MND_YAML_PLAIN, "key", "key");
    check_value("'k': v\n", 0, 0, false, MND_YAML_SINGLE_QUOTED, "k", "'k'");
}

/* フロー文脈の終端（, [ ] { } と : ）と継続行、flow の印 */
NAOS_TEST(scalar_flow_context_termination)
{
    check_value("[a b, c]", 1, 0, true, MND_YAML_PLAIN, "a b", "a b");
    check_value("{k: v}", 1, 0, true, MND_YAML_PLAIN, "k", "k");
    check_value("{k: v}", 4, 0, true, MND_YAML_PLAIN, "v", "v");
    check_value("[a:b]", 1, 0, true, MND_YAML_PLAIN, "a:b", "a:b");
    check_value("{k:, x}", 1, 0, true, MND_YAML_PLAIN, "k", "k");
    check_value("[a\n  b\n\n c,\n d]", 1, 0, true, MND_YAML_PLAIN, "a b\nc", "a\n  b\n\n c");
    check_value("[a\n]", 1, 0, true, MND_YAML_PLAIN, "a", "a");
    check_value("['a', \"b\"]", 1, 0, true, MND_YAML_SINGLE_QUOTED, "a", "'a'");
    check_value("['a', \"b\"]", 6, 0, true, MND_YAML_DOUBLE_QUOTED, "b", "\"b\"");
}

/* 解析する範囲が文書の途中から始まっても、範囲と行番号は文書全体の先頭を基準にする */
NAOS_TEST(scalar_lines_are_document_based)
{
    const char *text = "---\nk: 'a\n  b'\n---\n";
    mnd_yaml_fixture_t fx;
    mnd_span_t content;
    mnd_yaml_cursor_t at;
    mnd_yaml_node_t *node = NULL;

    content.start = 4;
    content.end = 15;
    NAOS_REQUIRE_TRUE(mnd_yaml_fixture_init(&fx, text, strlen(text), content, NULL));
    at = mnd_yaml_fixture_cursor(&fx, 7);
    NAOS_REQUIRE_TRUE(mnd_yaml_scalar_parse(&fx.parser, &at, 0, false, &node));
    NAOS_REQUIRE_TRUE(node != NULL);
    NAOS_CHECK_UINT_EQ(7, node->span.start);
    NAOS_CHECK_UINT_EQ(14, node->span.end);
    NAOS_CHECK_UINT_EQ(2, node->start_line);
    NAOS_CHECK_UINT_EQ(3, node->end_line);
    NAOS_CHECK_UINT_EQ(1, at.line_index);
    NAOS_CHECK_STR_EQ("a b", node->as.scalar.value);
    mnd_yaml_doc_set_root(fx.parser.doc, node);
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(fx.parser.doc));
    mnd_yaml_fixture_cleanup(&fx);
}

/* 不正なエスケープ（\0 を含む）は 6.5 のエラー（その位置の行） */
NAOS_TEST(scalar_invalid_escape_errors)
{
    const char *message = "invalid escape sequence in double-quoted scalar";

    check_error("k: \"a\\qb\"\n", 3, 0, false, message, 1);
    check_error("k: \"a\\0\"\n", 3, 0, false, message, 1);
    check_error("k: \"\\x00\"\n", 3, 0, false, message, 1);
    check_error("k: \"\\u0000\"\n", 3, 0, false, message, 1);
    check_error("k: \"\\x4\"\n", 3, 0, false, message, 1);
    check_error("k: \"\\u12G4\"\n", 3, 0, false, message, 1);
    check_error("k: \"\\uD800\"\n", 3, 0, false, message, 1);
    check_error("k: \"\\U00110000\"\n", 3, 0, false, message, 1);
    check_error("a: 1\nk: \"a\n\n  b \\q\"\n", 8, 0, false, message, 4);
}

/* 閉じていない引用符は開きの引用符の行のエラー */
NAOS_TEST(scalar_unterminated_quote_errors)
{
    const char *message = "unterminated quoted scalar";

    check_error("k: 'abc\n  def\n", 3, 0, false, message, 1);
    check_error("a: 1\nk: \"abc\n", 8, 0, false, message, 2);
    check_error("k: \"abc\\\"", 3, 0, false, message, 1);
    check_error("k: \"abc\\", 3, 0, false, message, 1);
    check_error("k: 'it''", 3, 0, false, message, 1);
    check_error("['a, b]", 1, 0, true, message, 1);
}

/* ノードの先頭の &、*、! は非対応の構文、@ と ` は予約の指示文字（6.1、6.2） */
NAOS_TEST(scalar_unsupported_and_reserved_indicators)
{
    check_error("k: &a v\n", 3, 0, false, "unsupported YAML syntax: anchor", 1);
    check_error("k: *a\n", 3, 0, false, "unsupported YAML syntax: alias", 1);
    check_error("a: 1\nk: !!str v\n", 8, 0, false, "unsupported YAML syntax: tag", 2);
    check_error("[!x a]", 1, 0, true, "unsupported YAML syntax: tag", 1);
    check_error("? k\n", 0, 0, false, "unsupported YAML syntax: complex mapping key", 1);
    check_error("k: @x\n", 3, 0, false, "reserved indicator \"@\" cannot start a scalar", 1);
    check_error("k: `x`\n", 3, 0, false, "reserved indicator \"`\" cannot start a scalar", 1);
}

/* スカラーとして始められない位置、単独の CR、継続行のインデントのタブ（6.2、6.3） */
NAOS_TEST(scalar_other_syntax_errors)
{
    check_error("k: ]\n", 3, 0, false, "unexpected content after value", 1);
    check_error("k: %x\n", 3, 0, false, "unexpected content after value", 1);
    check_error("[a, , b]", 4, 0, true, "invalid flow collection entry", 1);
    check_error("[- a]", 1, 0, true, "invalid flow collection entry", 1);
    check_error("[|]", 1, 0, true, "invalid flow collection entry", 1);
    check_error("k: a\rb\n", 3, 0, false, "carriage return without line feed", 1);
    check_error("k: 'a\n  b\rc'\n", 3, 0, false, "carriage return without line feed", 2);
    check_error("k: a\n \tb\n", 3, 0, false, "tab character in indentation", 2);
    check_error("k: 'a\n\t b'\n", 3, 0, false, "tab character in indentation", 2);
}

/* メモリ不足では false と out of memory（行番号 0）を返し、片付けたあとに確保が残らない（6.7） */
NAOS_TEST(scalar_out_of_memory)
{
    static char text[4000];
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_fixture_t fx;
    mnd_span_t content;
    mnd_yaml_cursor_t at;
    mnd_yaml_node_t *node = NULL;
    const mnd_diag_t *d = NULL;
    size_t n;
    size_t failures = 0;
    bool ok = false;

    memset(text, 'a', sizeof(text) - 1);
    text[0] = '\'';
    text[sizeof(text) - 2] = '\'';
    text[sizeof(text) - 1] = '\0';
    content.start = 0;
    content.end = strlen(text);
    for (n = 1; (n < 100) && !ok; ++n)
    {
        mnd_yaml_test_alloc_init(&ta, 0);
        NAOS_REQUIRE_TRUE(mnd_yaml_fixture_init(&fx, text, strlen(text), content, &ta.allocator));
        ta.fail_at = ta.calls + n;
        at = mnd_yaml_fixture_cursor(&fx, 0);
        node = NULL;
        ok = mnd_yaml_scalar_parse(&fx.parser, &at, 0, false, &node);
        if (ok)
        {
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&fx.diags));
            NAOS_CHECK_UINT_EQ(sizeof(text) - 3, node->as.scalar.value_len);
        }
        else
        {
            ++failures;
            NAOS_CHECK_TRUE(node == NULL);
            NAOS_CHECK_UINT_EQ(0, at.offset);
            NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&fx.diags));
            d = mnd_diag_list_at(&fx.diags, 0);
            NAOS_CHECK_STR_EQ("out of memory", d->message);
            NAOS_CHECK_UINT_EQ(0, d->line);
        }
        mnd_yaml_fixture_cleanup(&fx);
        NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));
    }
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_GE(failures, 1);
}

/*
 * ブロックスカラーの期待値。chomp・指示子・内容のインデントと、走査のあとのカーソルの位置
 * （消費した最後の行の次の行の先頭。範囲の終わりでは範囲の終わり）
 */
typedef struct block_expect
{
    mnd_yaml_style_t style;
    mnd_yaml_chomp_t chomp;
    bool has_indent_indicator;
    unsigned indent_indicator;
    size_t content_indent;
    size_t next_offset;
} block_expect_t;

static block_expect_t block_expect(mnd_yaml_style_t style, mnd_yaml_chomp_t chomp, unsigned indicator,
                                   size_t content_indent, size_t next_offset)
{
    block_expect_t e;

    e.style = style;
    e.chomp = chomp;
    e.has_indent_indicator = (indicator != 0);
    e.indent_indicator = indicator;
    e.content_indent = content_indent;
    e.next_offset = next_offset;

    return e;
}

/*
 * text の offset から読んだブロックスカラーの各欄・論理値・範囲（原文 raw）を確かめ、
 * カーソルが次の行の先頭にあること、診断がないこと、木の検査を満たすことを確かめる
 */
static void check_block(const char *text, size_t offset, size_t parent_indent, block_expect_t e, const char *value,
                        const char *raw)
{
    scalar_run_t r;
    size_t raw_len = strlen(raw);
    size_t next_index;

    NAOS_REQUIRE_TRUE(scalar_run(&r, text, offset, parent_indent, false));
    NAOS_CHECK_TRUE(r.ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&r.fx.diags));
    if (r.ok && (r.node != NULL))
    {
        NAOS_CHECK_INT_EQ((int)MND_YAML_SCALAR, (int)r.node->kind);
        NAOS_CHECK_INT_EQ((int)e.style, (int)r.node->as.scalar.style);
        NAOS_CHECK_FALSE(r.node->as.scalar.flow);
        NAOS_CHECK_INT_EQ((int)e.chomp, (int)r.node->as.scalar.chomp);
        NAOS_CHECK_TRUE(r.node->as.scalar.has_indent_indicator == e.has_indent_indicator);
        NAOS_CHECK_UINT_EQ(e.indent_indicator, r.node->as.scalar.indent_indicator);
        NAOS_CHECK_UINT_EQ(e.content_indent, r.node->as.scalar.content_indent);
        NAOS_CHECK_MEM_EQ(value, strlen(value), r.node->as.scalar.value, r.node->as.scalar.value_len);
        NAOS_CHECK_INT_EQ(0, (int)r.node->as.scalar.value[r.node->as.scalar.value_len]);
        NAOS_CHECK_UINT_EQ(offset, r.node->span.start);
        NAOS_CHECK_MEM_EQ(raw, raw_len, text + r.node->span.start, r.node->span.end - r.node->span.start);
        NAOS_CHECK_UINT_EQ(e.next_offset, r.at.offset);
        next_index = r.fx.parser.lines.count;
        if (e.next_offset < strlen(text))
        {
            next_index = mnd_yaml_lines_index_of(&r.fx.parser.lines, e.next_offset);
            NAOS_CHECK_UINT_EQ(r.fx.parser.lines.items[next_index].start, e.next_offset);
        }
        NAOS_CHECK_UINT_EQ(next_index, r.at.line_index);
        mnd_yaml_doc_set_root(r.fx.parser.doc, r.node);
        NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(r.fx.parser.doc));
    }
    else
    {
        NAOS_CHECK_TRUE(r.node != NULL);
    }
    mnd_yaml_fixture_cleanup(&r.fx);
}

/* ブロックスカラーのヘッダの判定 */
NAOS_TEST(scalar_block_header_detection)
{
    mnd_yaml_fixture_t fx;
    mnd_span_t content;
    const char *text = "a: |\nb: >-\nc: x|\nd:\n";

    content.start = 0;
    content.end = strlen(text);
    NAOS_REQUIRE_TRUE(mnd_yaml_fixture_init(&fx, text, strlen(text), content, NULL));
    NAOS_CHECK_TRUE(mnd_yaml_scalar_is_block_header(&fx.parser, mnd_yaml_fixture_cursor(&fx, 3)));
    NAOS_CHECK_TRUE(mnd_yaml_scalar_is_block_header(&fx.parser, mnd_yaml_fixture_cursor(&fx, 8)));
    NAOS_CHECK_FALSE(mnd_yaml_scalar_is_block_header(&fx.parser, mnd_yaml_fixture_cursor(&fx, 14)));
    NAOS_CHECK_FALSE(mnd_yaml_scalar_is_block_header(&fx.parser, mnd_yaml_fixture_cursor(&fx, 16)));
    NAOS_CHECK_FALSE(mnd_yaml_scalar_is_block_header(&fx.parser, mnd_yaml_fixture_cursor(&fx, 19)));
    NAOS_CHECK_FALSE(mnd_yaml_scalar_is_block_header(&fx.parser, mnd_yaml_fixture_cursor(&fx, strlen(text))));
    mnd_yaml_fixture_cleanup(&fx);
}

/* チョンプ・インデント指示子・内容のインデントの値と、各チョンプの論理値と範囲の終わり（3.2、3.4、3.8） */
NAOS_TEST(scalar_block_header_and_chomping)
{
    check_block("k: |\n  a\n  b\n\nx: 1\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 2, 14),
                "a\nb\n", "|\n  a\n  b");
    check_block("k: |-\n  a\n  b\n\nx: 1\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_STRIP, 0, 2, 15),
                "a\nb", "|-\n  a\n  b");
    check_block("k: |+\n  a\n  b\n\n\nx: 1\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_KEEP, 0, 2, 16),
                "a\nb\n\n\n", "|+\n  a\n  b\n\n");
    check_block("k: >\n  a\n  b\n\n  c\nx: 1\n", 3, 0, block_expect(MND_YAML_FOLDED, MND_YAML_CHOMP_CLIP, 0, 2, 18),
                "a b\nc\n", ">\n  a\n  b\n\n  c");
    check_block("k: >2\n   a\n  b\n", 3, 0, block_expect(MND_YAML_FOLDED, MND_YAML_CHOMP_CLIP, 2, 2, 15),
                " a\nb\n", ">2\n   a\n  b");
    check_block("k: |2-\n    a\n  b\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_STRIP, 2, 2, 17),
                "  a\nb", "|2-\n    a\n  b");
    /* 指示子は任意の順 */
    check_block("k: |-2\n    a\n  b\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_STRIP, 2, 2, 17),
                "  a\nb", "|-2\n    a\n  b");
    check_block("k: >+1\n a\n\n", 3, 0, block_expect(MND_YAML_FOLDED, MND_YAML_CHOMP_KEEP, 1, 1, 11),
                "a\n\n", ">+1\n a\n");
    /* インデント指示子は親のインデントに足す */
    check_block("  k: |1\n    a\n   b\n", 5, 2, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 1, 3, 19),
                " a\nb\n", "|1\n    a\n   b");
    /* 指示子がなければ最初の空でない行のインデント（親より深いこと） */
    check_block("- k: |\n      a\n      b\n  x: 1\n", 5, 2,
                block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 6, 23), "a\nb\n", "|\n      a\n      b");
}

/* ヘッダの後ろのコメント、空のブロックスカラー、行末記号のない最後の行 */
NAOS_TEST(scalar_block_header_comment_and_empty)
{
    check_block("k: | # c\n  a\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 2, 13), "a\n",
                "| # c\n  a");
    check_block("k: |\nx: 1\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 1, 5), "", "|");
    check_block("k: |-  # c\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_STRIP, 0, 1, 11), "", "|-");
    check_block("k: >", 3, 0, block_expect(MND_YAML_FOLDED, MND_YAML_CHOMP_CLIP, 0, 1, 4), "", ">");
    check_block("k: |\n\n\nx: 1\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 1, 7), "", "|");
    check_block("k: |+\n\n\nx: 1\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_KEEP, 0, 1, 8), "\n\n",
                "|+\n\n");
    check_block("k: |\n  a", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 2, 8), "a", "|\n  a");
    check_block("k: |+\n  a", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_KEEP, 0, 2, 9), "a", "|+\n  a");
    check_block("k: |+\n  a\n\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_KEEP, 0, 2, 11), "a\n\n",
                "|+\n  a\n");
    /* 浅いコメントの行で終わり、内容のインデント以上の # は内容 */
    check_block("k: |\n  a\n  # b\n# c\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 2, 15),
                "a\n# b\n", "|\n  a\n  # b");
}

/* リテラルの改行、先行と途中の空行、より深い行、内容のインデントのあとのタブと空白（3.8） */
NAOS_TEST(scalar_block_literal_value)
{
    check_block("k: |\n\n  a\n\n    b\n  c\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 2, 21),
                "\na\n\n  b\nc\n", "|\n\n  a\n\n    b\n  c");
    check_block("k: |\n  a\n    \n  b\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 2, 18),
                "a\n  \nb\n", "|\n  a\n    \n  b");
    check_block("k: |\n  a\n  \tb\n \n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 2, 16),
                "a\n\tb\n", "|\n  a\n  \tb");
    /* 指示子があれば、内容のインデントより深い先行の空白だけの行は内容 */
    check_block("k: |2\n    \n  b\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 2, 2, 15),
                "  \nb\n", "|2\n    \n  b");
    /* 内容のインデントより浅い空でない行で終わる */
    check_block("k: |\n    a\n  b\n", 3, 0, block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 4, 11), "a\n",
                "|\n    a");
}

/* 折り返し: 行の境目は空白、空行と、より深い行の前後は改行を残す（3.8。YAML 1.2 の例 8.10） */
NAOS_TEST(scalar_block_folded_value)
{
    check_block(">\n\n folded\n line\n\n next\n line\n   * bullet\n\n   * list\n   * lines\n\n last\n line\n\n"
                "# Comment\n",
                0, 0, block_expect(MND_YAML_FOLDED, MND_YAML_CHOMP_CLIP, 0, 1, 78),
                "\nfolded line\nnext line\n  * bullet\n\n  * list\n  * lines\n\nlast line\n",
                ">\n\n folded\n line\n\n next\n line\n   * bullet\n\n   * list\n   * lines\n\n last\n line");
    check_block("k: >-\n  a\n  b\n\n\n  c\n  \td\n  e\n", 3, 0,
                block_expect(MND_YAML_FOLDED, MND_YAML_CHOMP_STRIP, 0, 2, 29), "a b\n\nc\n\td\ne",
                ">-\n  a\n  b\n\n\n  c\n  \td\n  e");
    check_block("k: >\n  a\n\n    b\n\n  c\n", 3, 0, block_expect(MND_YAML_FOLDED, MND_YAML_CHOMP_CLIP, 0, 2, 21),
                "a\n\n  b\n\nc\n", ">\n  a\n\n    b\n\n  c");
}

/* CRLF の文書でも論理値の改行は LF で、範囲に最後の行末記号を含めない（3.9） */
NAOS_TEST(scalar_block_crlf_document)
{
    check_block("k: |\r\n  a\r\n  b\r\n\r\nx: 1\r\n", 3, 0,
                block_expect(MND_YAML_LITERAL, MND_YAML_CHOMP_CLIP, 0, 2, 18), "a\nb\n", "|\r\n  a\r\n  b");
    check_block("k: >+\r\n  a\r\n  b\r\n\r\nx: 1\r\n", 3, 0,
                block_expect(MND_YAML_FOLDED, MND_YAML_CHOMP_KEEP, 0, 2, 19), "a b\n\n", ">+\r\n  a\r\n  b\r\n");
}

/* 不正なヘッダ、深い先行の空白行、空行のインデントのタブ、単独の CR（6.2、6.3） */
NAOS_TEST(scalar_block_errors)
{
    const char *message = "invalid block scalar";

    check_error("k: |x\n  a\n", 3, 0, false, message, 1);
    check_error("k: |0\n  a\n", 3, 0, false, message, 1);
    check_error("k: |--\n  a\n", 3, 0, false, message, 1);
    check_error("k: |+-\n  a\n", 3, 0, false, message, 1);
    check_error("k: |22\n  a\n", 3, 0, false, message, 1);
    check_error("k: |10\n  a\n", 3, 0, false, message, 1);
    check_error("k: |#c\n  a\n", 3, 0, false, message, 1);
    check_error("k: >1 x\n  a\n", 3, 0, false, message, 1);
    check_error("k: | -\n  a\n", 3, 0, false, message, 1);
    check_error("a: 1\nk: >2-+\n", 8, 0, false, message, 2);
    /* 指示子がないのに、先行の空白だけの行が最初の空でない行より深い */
    check_error("a: 1\nk: |\n    \n  b\n", 8, 0, false, message, 2);
    check_error("k: >\n\n   \n  b\n", 3, 0, false, message, 1);
    check_error("k: |\n  a\n\t\n  b\n", 3, 0, false, "tab character in indentation", 3);
    check_error("k: |\n \t\n  b\n", 3, 0, false, "tab character in indentation", 2);
    check_error("k: |\n  a\rb\n", 3, 0, false, "carriage return without line feed", 2);
    check_error("k: | # a\rb\n  a\n", 3, 0, false, "carriage return without line feed", 1);
}

/* ブロックスカラーのメモリ不足でも false と out of memory を返し、確保が残らない（6.7） */
NAOS_TEST(scalar_block_out_of_memory)
{
    static char text[4000];
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_fixture_t fx;
    mnd_span_t content;
    mnd_yaml_cursor_t at;
    mnd_yaml_node_t *node = NULL;
    const mnd_diag_t *d = NULL;
    size_t n;
    size_t failures = 0;
    bool ok = false;

    memset(text, 'a', sizeof(text) - 1);
    text[0] = '>';
    text[1] = '\n';
    text[2] = ' ';
    text[sizeof(text) - 1] = '\0';
    content.start = 0;
    content.end = strlen(text);
    for (n = 1; (n < 100) && !ok; ++n)
    {
        mnd_yaml_test_alloc_init(&ta, 0);
        NAOS_REQUIRE_TRUE(mnd_yaml_fixture_init(&fx, text, strlen(text), content, &ta.allocator));
        ta.fail_at = ta.calls + n;
        at = mnd_yaml_fixture_cursor(&fx, 0);
        node = NULL;
        ok = mnd_yaml_scalar_parse(&fx.parser, &at, 0, false, &node);
        if (ok)
        {
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&fx.diags));
            NAOS_CHECK_UINT_EQ(sizeof(text) - 4, node->as.scalar.value_len);
        }
        else
        {
            ++failures;
            NAOS_CHECK_TRUE(node == NULL);
            NAOS_CHECK_UINT_EQ(0, at.offset);
            NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&fx.diags));
            d = mnd_diag_list_at(&fx.diags, 0);
            NAOS_CHECK_STR_EQ("out of memory", d->message);
            NAOS_CHECK_UINT_EQ(0, d->line);
        }
        mnd_yaml_fixture_cleanup(&fx);
        NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));
    }
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_GE(failures, 1);
}

NAOS_SUITE(scalar,
NAOS_ADD_TEST(scalar_scaffold_parser_reports_first_error_only),
    NAOS_ADD_TEST(scalar_fixture_builds_and_cleans_up),
    NAOS_ADD_TEST(scalar_span_and_style_of_each_style),
    NAOS_ADD_TEST(scalar_double_quoted_escapes),
    NAOS_ADD_TEST(scalar_double_quoted_escaped_tab),
    NAOS_ADD_TEST(scalar_single_quoted_doubled_quote),
    NAOS_ADD_TEST(scalar_multi_line_plain_folding),
    NAOS_ADD_TEST(scalar_multi_line_quoted_folding),
    NAOS_ADD_TEST(scalar_double_quoted_escaped_line_break),
    NAOS_ADD_TEST(scalar_crlf_document_uses_lf_in_values),
    NAOS_ADD_TEST(scalar_values_are_strings),
    NAOS_ADD_TEST(scalar_plain_indicators_inside_value),
    NAOS_ADD_TEST(scalar_flow_context_termination),
    NAOS_ADD_TEST(scalar_lines_are_document_based),
    NAOS_ADD_TEST(scalar_invalid_escape_errors),
    NAOS_ADD_TEST(scalar_unterminated_quote_errors),
    NAOS_ADD_TEST(scalar_unsupported_and_reserved_indicators),
    NAOS_ADD_TEST(scalar_other_syntax_errors),
    NAOS_ADD_TEST(scalar_out_of_memory),
    NAOS_ADD_TEST(scalar_block_header_detection),
    NAOS_ADD_TEST(scalar_block_header_and_chomping),
    NAOS_ADD_TEST(scalar_block_header_comment_and_empty),
    NAOS_ADD_TEST(scalar_block_literal_value),
    NAOS_ADD_TEST(scalar_block_folded_value),
    NAOS_ADD_TEST(scalar_block_crlf_document),
    NAOS_ADD_TEST(scalar_block_errors),
    NAOS_ADD_TEST(scalar_block_out_of_memory)
)
