/* スイート check: 構造の検査（StructureCheck）（4.11） */

#include <naos/naos.h>

#include "tu/tu_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* line_index に入れない値の印 */
#define CHECK_UNSET ((size_t)12345)

/* text（NUL 終端）を検査した結果が expected で、*line_index が expected_line であることを検査する */
static void check_text(const char *text, bool is_atx_heading, mnd_tu_check_result_t expected,
                       size_t expected_line)
{
    size_t line_index = CHECK_UNSET;
    mnd_tu_check_result_t result = mnd_tu_check_structure(text, strlen(text), is_atx_heading, &line_index);

    NAOS_CHECK_INT_EQ((int)expected, (int)result);
    NAOS_CHECK_UINT_EQ(expected_line, line_index);
}

/* text（NUL 終端）が見出しでない訳文として検出されないことを検査する */
static void check_ok(const char *text)
{
    check_text(text, false, MND_TU_CHECK_OK, CHECK_UNSET);
}

/* text（NUL 終端）が見出しでない訳文として line 行目のブロックの開始を検出することを検査する */
static void check_marker(const char *text, size_t line)
{
    check_text(text, false, MND_TU_CHECK_BLOCK_MARKER, line);
}

/* 通常の 1 行の文は構造を変えない */
NAOS_TEST(check_plain_sentence_is_ok)
{
    check_ok("Hello, world.");
    check_ok("これは通常の文です。");
}

/* 複数行の通常の文も検出しない */
NAOS_TEST(check_plain_multiline_is_ok)
{
    check_ok("first line\nsecond line\nthird line");
}

/* 空の行を含む訳文は空行として検出する（line_index は変えない） */
NAOS_TEST(check_blank_line)
{
    check_text("a\n\nb", false, MND_TU_CHECK_BLANK_LINE, CHECK_UNSET);
    check_text("\na", false, MND_TU_CHECK_BLANK_LINE, CHECK_UNSET);
    check_text("a\n", false, MND_TU_CHECK_BLANK_LINE, CHECK_UNSET);
}

/* 空白・タブだけの行も空行として検出する */
NAOS_TEST(check_whitespace_only_line)
{
    check_text("a\n   \nb", false, MND_TU_CHECK_BLANK_LINE, CHECK_UNSET);
    check_text("a\n\t \t\nb", false, MND_TU_CHECK_BLANK_LINE, CHECK_UNSET);
    check_text("  ", false, MND_TU_CHECK_BLANK_LINE, CHECK_UNSET);
}

/* 空行は表の先頭の項目なので、ブロックの開始より後ろの行にあっても空行が優先される */
NAOS_TEST(check_blank_line_wins_over_marker)
{
    check_text("# a\n\nb", false, MND_TU_CHECK_BLANK_LINE, CHECK_UNSET);
    check_text("a\n\nb", true, MND_TU_CHECK_BLANK_LINE, CHECK_UNSET);
}

/* ATX 見出しの訳文の改行は検出し、ATX 見出しでなければ検出しない */
NAOS_TEST(check_heading_line_break)
{
    check_text("first\nsecond", true, MND_TU_CHECK_HEADING_LINE_BREAK, CHECK_UNSET);
    check_text("first\n# second", true, MND_TU_CHECK_HEADING_LINE_BREAK, CHECK_UNSET);
    check_ok("first\nsecond");
    check_text("one line", true, MND_TU_CHECK_OK, CHECK_UNSET);
}

/* ATX 見出しの記号: # 1〜6 個の後ろが空白・タブ・行末 */
NAOS_TEST(check_atx_heading_marker)
{
    check_marker("# a", 0);
    check_marker("###### a", 0);
    check_marker("##\ta", 0);
    check_marker("#", 0);
    check_marker("###", 0);
    check_marker("text\n## b", 1);
    check_ok("#foo");
    check_ok("####### a");
    check_ok("#######");
    check_ok("a # b");
}

/* 引用の記号: > の後ろは問わない */
NAOS_TEST(check_block_quote_marker)
{
    check_marker(">", 0);
    check_marker("> quote", 0);
    check_marker(">quote", 0);
    check_marker("a\nb\n>c", 2);
    check_ok("a > b");
}

/* 箇条書きの記号: - / + / * の後ろが空白・タブ・行末 */
NAOS_TEST(check_bullet_marker)
{
    check_marker("- item", 0);
    check_marker("+ item", 0);
    check_marker("* item", 0);
    check_marker("-\titem", 0);
    check_marker("-", 0);
    check_marker("a\n+", 1);
    check_ok("*強調*");
    check_ok("-1 度");
    check_ok("--");
    check_ok("+1");
}

/* 番号付きの記号: 1〜9 桁の数字と . / ) の後ろが空白・タブ・行末 */
NAOS_TEST(check_ordered_marker)
{
    check_marker("1) item", 0);
    check_marker("1. item", 0);
    check_marker("123456789. item", 0);
    check_marker("2)", 0);
    check_marker("3.\titem", 0);
    check_marker("a\n10. b", 1);
    check_ok("1.5");
    check_ok("1234567890. item");
    check_ok("1x item");
    check_ok("2024 年");
    check_ok(". a");
}

/* フェンスの記号: ` または ~ 3 個以上（後ろは問わない） */
NAOS_TEST(check_fence_marker)
{
    check_marker("```", 0);
    check_marker("```c", 0);
    check_marker("~~~", 0);
    check_marker("~~~~ text", 0);
    check_marker("a\n````", 1);
    check_ok("``code``");
    check_ok("~~");
    check_ok("`~~");
}

/* 区切り線: 同じ - / * / _ が 3 個以上（空白・タブを挟んでよい） */
NAOS_TEST(check_thematic_break_marker)
{
    check_marker("- - -", 0);
    check_marker("***", 0);
    check_marker("___", 0);
    check_marker("_ _ _", 0);
    check_marker("* * *  ", 0);
    check_marker("---", 0);
    check_ok("__");
    check_ok("_ _");
    check_ok("_*_");
    check_ok("___a");
    check_ok("__init__");
}

/* setext の下線: = だけ 1 個以上（末尾の空白・タブは許す） */
NAOS_TEST(check_setext_underline_marker)
{
    check_marker("===", 0);
    check_marker("=", 0);
    check_marker("a\n==  ", 1);
    check_ok("= a");
    check_ok("==a");
    check_ok("a = b");
}

/* 先頭の 0〜3 個の空白のあとの記号は検出し、4 個以上（インデントのコード）やタブのあとは検出しない */
NAOS_TEST(check_leading_spaces)
{
    check_marker("   - item", 0);
    check_marker(" # a", 0);
    check_marker("  > a", 0);
    check_marker("   ===", 0);
    check_marker("a\n   1. b", 1);
    check_ok("    - item");
    check_ok("    # a");
    check_ok("\t- item");
}

/* 最初に当てはまった行の番号を返す */
NAOS_TEST(check_first_marker_line)
{
    check_marker("a\nb\n- c\n# d", 2);
    check_marker("# a\n- b", 0);
}

/* 長さで区切った範囲だけを検査する（範囲の外の改行や記号は見ない） */
NAOS_TEST(check_respects_length)
{
    static const char text[] = "abc\n# d";
    size_t line_index = CHECK_UNSET;

    NAOS_CHECK_INT_EQ((int)MND_TU_CHECK_OK, (int)mnd_tu_check_structure(text, 3, false, &line_index));
    NAOS_CHECK_INT_EQ((int)MND_TU_CHECK_OK, (int)mnd_tu_check_structure(text, 3, true, &line_index));
    NAOS_CHECK_UINT_EQ(CHECK_UNSET, line_index);
    NAOS_CHECK_INT_EQ((int)MND_TU_CHECK_BLOCK_MARKER,
                      (int)mnd_tu_check_structure(text, sizeof(text) - 1, false, &line_index));
    NAOS_CHECK_UINT_EQ((size_t)1, line_index);
}

NAOS_SUITE(check,
    NAOS_ADD_TEST(check_plain_sentence_is_ok),
    NAOS_ADD_TEST(check_plain_multiline_is_ok),
    NAOS_ADD_TEST(check_blank_line),
    NAOS_ADD_TEST(check_whitespace_only_line),
    NAOS_ADD_TEST(check_blank_line_wins_over_marker),
    NAOS_ADD_TEST(check_heading_line_break),
    NAOS_ADD_TEST(check_atx_heading_marker),
    NAOS_ADD_TEST(check_block_quote_marker),
    NAOS_ADD_TEST(check_bullet_marker),
    NAOS_ADD_TEST(check_ordered_marker),
    NAOS_ADD_TEST(check_fence_marker),
    NAOS_ADD_TEST(check_thematic_break_marker),
    NAOS_ADD_TEST(check_setext_underline_marker),
    NAOS_ADD_TEST(check_leading_spaces),
    NAOS_ADD_TEST(check_first_marker_line),
    NAOS_ADD_TEST(check_respects_length)
)
