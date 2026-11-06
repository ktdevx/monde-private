/* Text: 論理的な文字列とアンエスケープ */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "markdown/md.h"
#include "markdown/md_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * 引用の中の 3 行の段落（プレフィックス、補う空白、CRLF を含む）:
 *   0: "> foo\r\n"   prefix [0, 2)、content [2, 5)、eol 2
 *   7: ">\tbar\r\n"  prefix [7, 9)（> とタブ。タブの 2 桁分が残る）、content [9, 12)、pad 2、eol 2
 *  14: "> baz"       prefix [14, 16)、content [16, 19)、eol 0
 */
static const char QUOTE_DATA[] = "> foo\r\n>\tbar\r\n> baz";
#define QUOTE_LEN (sizeof(QUOTE_DATA) - 1)

/* 空行を挟むコードブロックの 3 行（"a"、""、"b"） */
static const char CODE_DATA[] = "a\n\nb\n";
#define CODE_LEN (sizeof(CODE_DATA) - 1)

static mnd_span_t span_of(size_t start, size_t end)
{
    mnd_span_t span;

    span.start = start;
    span.end = end;

    return span;
}

static mnd_md_line_t line_of(size_t line, size_t prefix_start, size_t content_start, size_t content_end,
                             unsigned char pad, unsigned char eol_len)
{
    mnd_md_line_t result;

    memset(&result, 0, sizeof(result));
    result.line = line;
    result.prefix = span_of(prefix_start, content_start);
    result.content = span_of(content_start, content_end);
    result.pad = pad;
    result.eol_len = eol_len;

    return result;
}

/* 文書とリーフ（行の配列をアリーナに複製する）を作る。lines が NULL なら行を持たないリーフ */
static mnd_md_node_t *make_leaf(mnd_md_doc_t *doc, mnd_md_kind_t kind, mnd_span_t span,
                                const mnd_md_line_t *lines, size_t line_count)
{
    mnd_md_node_t *leaf = mnd_md_node_new(doc, kind, span);

    if ((leaf == NULL) || (lines == NULL))
    {
        return leaf;
    }
    leaf->lines = mnd_md_arena_dup(&doc->arena, lines, line_count * sizeof(lines[0]));
    if (leaf->lines == NULL)
    {
        return NULL;
    }
    leaf->line_count = line_count;

    return leaf;
}

static void make_quote_lines(mnd_md_line_t lines[3])
{
    lines[0] = line_of(1, 0, 2, 5, 0, 2);
    lines[1] = line_of(2, 7, 9, 12, 2, 2);
    lines[2] = line_of(3, 14, 16, 19, 0, 0);
}

/* 引用の段落のリーフについて、span の論理的な文字列が expected と一致することを確かめる */
static void check_quote_text(size_t start, size_t end, const char *expected)
{
    mnd_md_line_t lines[3];
    mnd_md_doc_t *doc = mnd_md_doc_create(QUOTE_DATA, QUOTE_LEN, NULL);
    mnd_md_node_t *leaf;
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_NOT_NULL(doc);
    make_quote_lines(lines);
    leaf = make_leaf(doc, MND_MD_PARAGRAPH, span_of(2, 19), lines, 3);
    NAOS_REQUIRE_NOT_NULL(leaf);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span_of(start, end), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, expected, strlen(expected));
    mnd_buf_free(&out);
    mnd_md_doc_free(doc);
}

/* text の unescape の結果が expected[0..expected_len) と一致することを確かめる */
static void check_unescape_n(const char *text, const char *expected, size_t expected_len)
{
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_CHECK_TRUE(mnd_md_unescape(text, strlen(text), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, expected, expected_len);
    mnd_buf_free(&out);
}

static void check_unescape(const char *text, const char *expected)
{
    check_unescape_n(text, expected, strlen(expected));
}

NAOS_TEST(three_line_leaf_skips_prefix_adds_pad_and_joins_with_lf)
{
    check_quote_text(2, 19, "foo\n  bar\nbaz");
}

NAOS_TEST(span_inside_middle_line)
{
    /* 内容の先頭を含まないので、補う空白は入らない */
    check_quote_text(10, 11, "a");
    check_quote_text(10, 12, "ar");
}

NAOS_TEST(span_covering_prefix_bytes_includes_pad)
{
    /* プレフィックスから始まる範囲は内容の先頭を含むので、補う空白が入る */
    check_quote_text(7, 12, "  bar");
    check_quote_text(8, 10, "  b");
    check_quote_text(0, 5, "foo");
}

NAOS_TEST(span_across_lines)
{
    check_quote_text(3, 10, "oo\n  b");
    check_quote_text(4, 17, "o\n  bar\nb");
}

NAOS_TEST(span_ending_at_line_end_has_no_trailing_lf)
{
    check_quote_text(2, 5, "foo");
    /* 次の行の内容の先頭に届かない範囲は、行の境目を覆わないので改行は入らない */
    check_quote_text(2, 8, "foo");
    /* 前の行の内容の終わりに届かない位置から始まる範囲も、行の境目を覆わない */
    check_quote_text(6, 12, "  bar");
}

NAOS_TEST(span_covering_line_boundary_keeps_lf)
{
    /* 行 i の内容の終わりから行 i + 1 の内容の始まりまでを覆う範囲は、その境目の "\n" を含む */
    check_quote_text(2, 9, "foo\n");
    check_quote_text(5, 12, "\n  bar");
    check_quote_text(5, 16, "\n  bar\n");
    check_quote_text(5, 19, "\n  bar\nbaz");
}

NAOS_TEST(span_only_on_prefix_and_eol_is_empty)
{
    /* 行の境目を覆う範囲は "\n" だけになる */
    check_quote_text(5, 9, "\n");
    check_quote_text(12, 16, "\n");
    /* 境目を覆わない範囲は空 */
    check_quote_text(6, 9, "");
    check_quote_text(5, 8, "");
    check_quote_text(3, 3, "");
    check_quote_text(5, 5, "");
}

/* 引用の外の 3 行（プレフィックスなし）のリーフで、span の論理的な文字列が expected と一致することを確かめる */
static void check_three_line_text(const char *data, const size_t bounds[6], mnd_span_t span,
                                  const char *expected)
{
    mnd_md_line_t lines[3];
    mnd_md_doc_t *doc = mnd_md_doc_create(data, strlen(data), NULL);
    mnd_md_node_t *leaf;
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_NOT_NULL(doc);
    lines[0] = line_of(1, bounds[0], bounds[0], bounds[1], 0, 1);
    lines[1] = line_of(2, bounds[2], bounds[2], bounds[3], 0, 1);
    lines[2] = line_of(3, bounds[4], bounds[4], bounds[5], 0, 0);
    leaf = make_leaf(doc, MND_MD_PARAGRAPH, span_of(bounds[0], bounds[5]), lines, 3);
    NAOS_REQUIRE_NOT_NULL(leaf);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span, &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, expected, strlen(expected));
    mnd_buf_free(&out);
    mnd_md_doc_free(doc);
}

NAOS_TEST(inline_span_starting_and_ending_at_line_boundary)
{
    /* 画像 "![\nfoo\n](x)" の alt の範囲 [2, 7) は、前後の行の境目を含む */
    static const size_t image[6] = {0, 2, 3, 6, 7, 11};
    /* コードスパン "`\nfoo\n`" の内容の範囲 [1, 6) */
    static const size_t code[6] = {0, 1, 2, 5, 6, 7};

    check_three_line_text("![\nfoo\n](x)", image, span_of(2, 7), "\nfoo\n");
    check_three_line_text("`\nfoo\n`", code, span_of(1, 6), "\nfoo\n");
    /* 境目の片側だけ */
    check_three_line_text("`\nfoo\n`", code, span_of(1, 5), "\nfoo");
    check_three_line_text("`\nfoo\n`", code, span_of(2, 6), "foo\n");
}

NAOS_TEST(span_is_clamped_to_doc_length)
{
    check_quote_text(2, 1000, "foo\n  bar\nbaz");
    check_quote_text(1000, 2000, "");
}

NAOS_TEST(empty_line_inside_span_is_kept)
{
    mnd_md_line_t lines[3];
    mnd_md_doc_t *doc = mnd_md_doc_create(CODE_DATA, CODE_LEN, NULL);
    mnd_md_node_t *leaf;
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_NOT_NULL(doc);
    lines[0] = line_of(1, 0, 0, 1, 0, 1);
    lines[1] = line_of(2, 2, 2, 2, 0, 1);
    lines[2] = line_of(3, 3, 3, 4, 0, 1);
    leaf = make_leaf(doc, MND_MD_CODE_BLOCK, span_of(0, 4), lines, 3);
    NAOS_REQUIRE_NOT_NULL(leaf);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span_of(0, 5), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, "a\n\nb", 4);
    /* 空の行の位置で終わる範囲は、その行を含める */
    mnd_buf_clear(&out);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span_of(0, 2), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, "a\n", 2);
    /* 内容の終わりから始まる範囲も、覆う境目ごとに "\n" を含む（空の行の内容は空） */
    mnd_buf_clear(&out);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span_of(1, 3), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, "\n\n", 2);
    mnd_buf_clear(&out);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span_of(1, 2), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, "\n", 1);
    mnd_buf_clear(&out);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span_of(2, 3), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, "\n", 1);
    mnd_buf_free(&out);
    mnd_md_doc_free(doc);
}

/* data のリーフ（span、行 lines）を leaf->span で文字列にした結果が expected と一致することを確かめる */
static void check_leaf_span_text(const char *data, mnd_span_t span, const mnd_md_line_t *lines,
                                 size_t line_count, const char *expected)
{
    mnd_md_doc_t *doc = mnd_md_doc_create(data, strlen(data), NULL);
    mnd_md_node_t *leaf;
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_NOT_NULL(doc);
    leaf = make_leaf(doc, MND_MD_CODE_BLOCK, span, lines, line_count);
    NAOS_REQUIRE_NOT_NULL(leaf);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, leaf->span, &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, expected, strlen(expected));
    mnd_buf_free(&out);
    mnd_md_doc_free(doc);
}

NAOS_TEST(last_empty_line_of_leaf_is_kept)
{
    /*
     * 閉じのないフェンスコードブロック "```\na\n\n":
     * 範囲は開きフェンスから最後の行の line_end（6）まで。行は "a"（[4, 5)）と ""（[6, 6)）
     */
    mnd_md_line_t lines[2];

    lines[0] = line_of(2, 4, 4, 5, 0, 1);
    lines[1] = line_of(3, 6, 6, 6, 0, 1);
    check_leaf_span_text("```\na\n\n", span_of(0, 6), lines, 2, "a\n");
}

NAOS_TEST(last_empty_quoted_line_of_leaf_is_kept)
{
    /*
     * 引用の中の閉じのないフェンスコードブロック "> ```\n> a\n>\n":
     * 範囲は [2, 11)。行は prefix [6, 8)・content [8, 9) と、`>` だけの行の prefix [10, 11)・content [11, 11)
     */
    mnd_md_line_t lines[2];

    lines[0] = line_of(2, 6, 8, 9, 0, 1);
    lines[1] = line_of(3, 10, 11, 11, 0, 1);
    check_leaf_span_text("> ```\n> a\n>\n", span_of(2, 11), lines, 2, "a\n");
}

NAOS_TEST(leaf_without_lines_appends_raw_bytes)
{
    mnd_md_doc_t *doc = mnd_md_doc_create(QUOTE_DATA, QUOTE_LEN, NULL);
    mnd_md_node_t *leaf;
    mnd_md_line_t lines[3];
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_NOT_NULL(doc);
    leaf = make_leaf(doc, MND_MD_THEMATIC_BREAK, span_of(0, 19), NULL, 0);
    NAOS_REQUIRE_NOT_NULL(leaf);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span_of(0, 9), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, "> foo\r\n>\t", 9);
    /* 内容の長さで切り詰める */
    mnd_buf_clear(&out);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span_of(16, 100), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, "baz", 3);
    /* lines があっても line_count が 0 なら行を持たない */
    make_quote_lines(lines);
    leaf->lines = lines;
    leaf->line_count = 0;
    mnd_buf_clear(&out);
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span_of(0, 5), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, "> foo", 5);
    leaf->lines = NULL;
    mnd_buf_free(&out);
    mnd_md_doc_free(doc);
}

NAOS_TEST(logical_text_appends_after_existing_content)
{
    mnd_md_line_t lines[3];
    mnd_md_doc_t *doc = mnd_md_doc_create(QUOTE_DATA, QUOTE_LEN, NULL);
    mnd_md_node_t *leaf;
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_NOT_NULL(doc);
    make_quote_lines(lines);
    leaf = make_leaf(doc, MND_MD_PARAGRAPH, span_of(2, 19), lines, 3);
    NAOS_REQUIRE_NOT_NULL(leaf);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "X"));
    NAOS_CHECK_TRUE(mnd_md_logical_text(doc, leaf, span_of(16, 19), &out));
    NAOS_CHECK_STR_EQ("Xbaz", mnd_buf_cstr(&out));
    mnd_buf_free(&out);
    mnd_md_doc_free(doc);
}

NAOS_TEST(unescape_backslash_escapes)
{
    check_unescape("\\*", "*");
    check_unescape("\\a", "\\a");
    check_unescape("\\\\", "\\");
    check_unescape("\\", "\\");
    check_unescape("a\\", "a\\");
    check_unescape("\\!\\\"\\#\\$\\%\\&\\'\\(\\)\\*\\+\\,\\-\\.\\/\\:\\;\\<\\=\\>\\?\\@\\[\\\\\\]\\^\\_\\`\\{\\|\\}\\~",
                   "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");
    /* ASCII の句読点でない文字の前のバックスラッシュは残る */
    check_unescape("\\1\\ \\\xC3\xA9", "\\1\\ \\\xC3\xA9");
    /* エスケープした & は実体参照を始めない */
    check_unescape("\\&amp;", "&amp;");
}

NAOS_TEST(unescape_entity_references)
{
    check_unescape("&amp;", "&");
    check_unescape("a&lt;b&gt;c", "a<b>c");
    check_unescape("&ngE;", "\xE2\x89\xA7\xCC\xB8");
    check_unescape("&CounterClockwiseContourIntegral;", "\xE2\x88\xB3");
    check_unescape("&nosuch;", "&nosuch;");
    check_unescape("&amp", "&amp");
    check_unescape("&;", "&;");
    check_unescape("& amp;", "& amp;");
    check_unescape("&", "&");
    /* 表の最長より長い名前 */
    check_unescape("&CounterClockwiseContourIntegralX;", "&CounterClockwiseContourIntegralX;");
    check_unescape("&amp;&amp;", "&&");
}

NAOS_TEST(unescape_numeric_references)
{
    check_unescape("&#35;", "#");
    check_unescape("&#X22;", "\"");
    check_unescape("&#x22;", "\"");
    check_unescape("&#1234;", "\xD3\x92");
    check_unescape("&#x1F600;", "\xF0\x9F\x98\x80");
    check_unescape("&#9999;", "\xE2\x9C\x8F");
    check_unescape("&#0;", "\xEF\xBF\xBD");
    check_unescape("&#x0;", "\xEF\xBF\xBD");
    check_unescape("&#0000000;", "\xEF\xBF\xBD");
    check_unescape("&#xD800;", "\xEF\xBF\xBD");
    check_unescape("&#xDFFF;", "\xEF\xBF\xBD");
    check_unescape("&#1234567;", "\xEF\xBF\xBD");
    check_unescape("&#x110000;", "\xEF\xBF\xBD");
    check_unescape("&#x10FFFF;", "\xF4\x8F\xBF\xBF");
    /* 桁数の上限を超えるものと、数字のないものは文字参照ではない */
    check_unescape("&#12345678;", "&#12345678;");
    check_unescape("&#x1234567;", "&#x1234567;");
    check_unescape("&#;", "&#;");
    check_unescape("&#x;", "&#x;");
    check_unescape("&#87", "&#87");
    check_unescape("&#abcdef0;", "&#abcdef0;");
}

NAOS_TEST(unescape_keeps_nul_and_appends)
{
    static const char text[] = { 'a', '\0', '&', 'a', 'm', 'p', ';' };
    static const char expected[] = { 'X', 'a', '\0', '&' };
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "X"));
    NAOS_CHECK_TRUE(mnd_md_unescape(text, sizeof(text), &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, expected, sizeof(expected));
    mnd_buf_clear(&out);
    NAOS_CHECK_TRUE(mnd_md_unescape(NULL, 0, &out));
    NAOS_CHECK_UINT_EQ(0, out.len);
    mnd_buf_free(&out);
    check_unescape_n("", "", 0);
}

NAOS_SUITE(text,
    NAOS_ADD_TEST(three_line_leaf_skips_prefix_adds_pad_and_joins_with_lf),
    NAOS_ADD_TEST(span_inside_middle_line),
    NAOS_ADD_TEST(span_covering_prefix_bytes_includes_pad),
    NAOS_ADD_TEST(span_across_lines),
    NAOS_ADD_TEST(span_ending_at_line_end_has_no_trailing_lf),
    NAOS_ADD_TEST(span_covering_line_boundary_keeps_lf),
    NAOS_ADD_TEST(span_only_on_prefix_and_eol_is_empty),
    NAOS_ADD_TEST(inline_span_starting_and_ending_at_line_boundary),
    NAOS_ADD_TEST(span_is_clamped_to_doc_length),
    NAOS_ADD_TEST(empty_line_inside_span_is_kept),
    NAOS_ADD_TEST(last_empty_line_of_leaf_is_kept),
    NAOS_ADD_TEST(last_empty_quoted_line_of_leaf_is_kept),
    NAOS_ADD_TEST(leaf_without_lines_appends_raw_bytes),
    NAOS_ADD_TEST(logical_text_appends_after_existing_content),
    NAOS_ADD_TEST(unescape_backslash_escapes),
    NAOS_ADD_TEST(unescape_entity_references),
    NAOS_ADD_TEST(unescape_numeric_references),
    NAOS_ADD_TEST(unescape_keeps_nul_and_appends)
)
