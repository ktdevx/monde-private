/* スイート lines: 物理的な行の表（2.5、6.3、7.1、7.2） */

#include <naos/naos.h>

#include "test_alloc.h"
#include "yaml/yaml_internal.h"
#include "yaml/yaml_lines.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * text の [start, end) を既定の確保関数で行に分ける
 */
static bool build_range(const char *text, size_t start, size_t end, mnd_yaml_lines_t *out)
{
    mnd_span_t content;

    content.start = start;
    content.end = end;

    return mnd_yaml_lines_build(text, strlen(text), content, mnd_yaml_allocator_or_default(NULL), out);
}

static bool build_all(const char *text, mnd_yaml_lines_t *out)
{
    return build_range(text, 0, strlen(text), out);
}

static void free_lines(mnd_yaml_lines_t *lines)
{
    mnd_yaml_lines_free(mnd_yaml_allocator_or_default(NULL), lines);
}

/* 文書の途中から始まる範囲の最初の行番号は文書全体の先頭から数える（7.1） */
NAOS_TEST(lines_first_line_number_of_mid_document_range)
{
    const char *text = "a\nb\r\nc: 1\n  d\n";
    mnd_yaml_lines_t lines;

    NAOS_REQUIRE_TRUE(build_range(text, 5, strlen(text), &lines));
    NAOS_REQUIRE_UINT_EQ(2, lines.count);
    NAOS_CHECK_UINT_EQ(3, lines.first_line);
    NAOS_CHECK_UINT_EQ(5, lines.range.start);
    NAOS_CHECK_UINT_EQ(strlen(text), lines.range.end);
    NAOS_CHECK_UINT_EQ(3, lines.items[0].line);
    NAOS_CHECK_UINT_EQ(5, lines.items[0].start);
    NAOS_CHECK_UINT_EQ(5, lines.items[0].content.start);
    NAOS_CHECK_UINT_EQ(9, lines.items[0].content.end);
    NAOS_CHECK_UINT_EQ(1, lines.items[0].eol_len);
    NAOS_CHECK_UINT_EQ(4, lines.items[1].line);
    NAOS_CHECK_UINT_EQ(10, lines.items[1].start);
    NAOS_CHECK_UINT_EQ(2, lines.items[1].indent);
    free_lines(&lines);
}

/* インデントは行頭の空白の数。最後の行に行末記号がなければ eol_len は 0 */
NAOS_TEST(lines_indent_counts_and_last_line_without_eol)
{
    const char *text = "k:\n  v\n    w";
    mnd_yaml_lines_t lines;

    NAOS_REQUIRE_TRUE(build_all(text, &lines));
    NAOS_REQUIRE_UINT_EQ(3, lines.count);
    NAOS_CHECK_UINT_EQ(1, lines.first_line);
    NAOS_CHECK_UINT_EQ(0, lines.items[0].indent);
    NAOS_CHECK_UINT_EQ(2, lines.items[1].indent);
    NAOS_CHECK_UINT_EQ(4, lines.items[2].indent);
    NAOS_CHECK_UINT_EQ(1, lines.items[0].line);
    NAOS_CHECK_UINT_EQ(2, lines.items[1].line);
    NAOS_CHECK_UINT_EQ(3, lines.items[2].line);
    NAOS_CHECK_UINT_EQ(3, lines.items[1].start);
    NAOS_CHECK_UINT_EQ(3, lines.items[1].content.start);
    NAOS_CHECK_UINT_EQ(6, lines.items[1].content.end);
    NAOS_CHECK_UINT_EQ(7, lines.items[2].start);
    NAOS_CHECK_UINT_EQ(strlen(text), lines.items[2].content.end);
    NAOS_CHECK_UINT_EQ(0, lines.items[2].eol_len);
    NAOS_CHECK_FALSE(lines.items[1].blank);
    NAOS_CHECK_FALSE(lines.items[1].comment);
    NAOS_CHECK_FALSE(lines.items[1].tab_in_indent);
    free_lines(&lines);
}

/* 空白とタブだけの行と空の行は blank、行頭の空白とタブのあとが # の行は comment（2.5） */
NAOS_TEST(lines_blank_and_comment_detection)
{
    const char *text = "  \n\t \n# c\n  # d\nx # y\n\n\t# e\n";
    mnd_yaml_lines_t lines;
    static const bool blank[] = { true, true, false, false, false, true, false };
    static const bool comment[] = { false, false, true, true, false, false, true };
    size_t i;

    NAOS_REQUIRE_TRUE(build_all(text, &lines));
    NAOS_REQUIRE_UINT_EQ(7, lines.count);
    for (i = 0; i < 7; ++i)
    {
        NAOS_CHECK_UINT_EQ(i + 1, lines.items[i].line);
        NAOS_CHECK_TRUE(lines.items[i].blank == blank[i]);
        NAOS_CHECK_TRUE(lines.items[i].comment == comment[i]);
    }
    NAOS_CHECK_UINT_EQ(2, lines.items[3].indent);
    NAOS_CHECK_UINT_EQ(lines.items[5].start, lines.items[5].content.end);
    NAOS_CHECK_TRUE(lines.items[6].tab_in_indent);
    free_lines(&lines);
}

/* 行頭の空白とタブの並びにタブがあれば tab_in_indent（6.3）。内容の途中のタブは数えない */
NAOS_TEST(lines_tab_in_indent_flag)
{
    const char *text = "a:\n\tb\n  \tc\n  d\te\n";
    mnd_yaml_lines_t lines;

    NAOS_REQUIRE_TRUE(build_all(text, &lines));
    NAOS_REQUIRE_UINT_EQ(4, lines.count);
    NAOS_CHECK_FALSE(lines.items[0].tab_in_indent);
    NAOS_CHECK_TRUE(lines.items[1].tab_in_indent);
    NAOS_CHECK_UINT_EQ(0, lines.items[1].indent);
    NAOS_CHECK_TRUE(lines.items[2].tab_in_indent);
    NAOS_CHECK_UINT_EQ(2, lines.items[2].indent);
    NAOS_CHECK_FALSE(lines.items[3].tab_in_indent);
    NAOS_CHECK_UINT_EQ(2, lines.items[3].indent);
    NAOS_CHECK_FALSE(lines.items[1].blank);
    NAOS_CHECK_FALSE(lines.items[2].comment);
    free_lines(&lines);
}

/* CRLF の行の行末記号の長さは 2 で、内容に CR が入らない（7.2） */
NAOS_TEST(lines_crlf_eol_len_two_without_cr_in_content)
{
    const char *text = "a: 1\r\n  b\r\n\r\nc";
    mnd_yaml_lines_t lines;
    size_t i;
    size_t j;

    NAOS_REQUIRE_TRUE(build_all(text, &lines));
    NAOS_REQUIRE_UINT_EQ(4, lines.count);
    NAOS_CHECK_UINT_EQ(2, lines.items[0].eol_len);
    NAOS_CHECK_UINT_EQ(0, lines.items[0].content.start);
    NAOS_CHECK_UINT_EQ(4, lines.items[0].content.end);
    NAOS_CHECK_UINT_EQ(6, lines.items[1].start);
    NAOS_CHECK_UINT_EQ(2, lines.items[1].indent);
    NAOS_CHECK_UINT_EQ(9, lines.items[1].content.end);
    NAOS_CHECK_UINT_EQ(2, lines.items[1].eol_len);
    NAOS_CHECK_TRUE(lines.items[2].blank);
    NAOS_CHECK_UINT_EQ(2, lines.items[2].eol_len);
    NAOS_CHECK_UINT_EQ(0, lines.items[3].eol_len);
    NAOS_CHECK_UINT_EQ(4, lines.items[3].line);
    for (i = 0; i < lines.count; ++i)
    {
        for (j = lines.items[i].content.start; j < lines.items[i].content.end; ++j)
        {
            NAOS_CHECK_TRUE(text[j] != '\r');
        }
    }
    free_lines(&lines);
}

/* 単独の CR は行を分けず、行の内容に残る（7.2） */
NAOS_TEST(lines_lone_cr_does_not_split_lines)
{
    const char *text = "a\rb\nc\r";
    mnd_yaml_lines_t lines;

    NAOS_REQUIRE_TRUE(build_all(text, &lines));
    NAOS_REQUIRE_UINT_EQ(2, lines.count);
    NAOS_CHECK_UINT_EQ(0, lines.items[0].content.start);
    NAOS_CHECK_UINT_EQ(3, lines.items[0].content.end);
    NAOS_CHECK_UINT_EQ(1, lines.items[0].eol_len);
    NAOS_CHECK_UINT_EQ(4, lines.items[1].start);
    NAOS_CHECK_UINT_EQ(6, lines.items[1].content.end);
    NAOS_CHECK_UINT_EQ(0, lines.items[1].eol_len);
    NAOS_CHECK_UINT_EQ(2, lines.items[1].line);
    free_lines(&lines);
}

/* 空の範囲は行がなく、最初の行番号は有効。範囲が行末記号で終わっても後ろに空の行を作らない */
NAOS_TEST(lines_empty_range_and_trailing_eol)
{
    const char *text = "a\nb\n";
    mnd_yaml_lines_t lines;

    NAOS_REQUIRE_TRUE(build_range(text, 2, 2, &lines));
    NAOS_CHECK_UINT_EQ(0, lines.count);
    NAOS_CHECK_UINT_EQ(2, lines.first_line);
    NAOS_CHECK_UINT_EQ(2, mnd_yaml_lines_line_of(&lines, 2));
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_lines_index_of(&lines, 2));
    free_lines(&lines);

    NAOS_REQUIRE_TRUE(build_all(text, &lines));
    NAOS_CHECK_UINT_EQ(2, lines.count);
    free_lines(&lines);
    NAOS_CHECK_UINT_EQ(0, lines.count);
    NAOS_CHECK_TRUE(lines.items == NULL);
    free_lines(&lines); /* 解放済みの表に対しても安全 */
    NAOS_CHECK_UINT_EQ(0, lines.count);
}

/* 確保の失敗で false を返し、確保したものを残さず、表を空にする */
NAOS_TEST(lines_allocation_failure_returns_false)
{
    const char *text = "a\nb\nc\n";
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_lines_t lines;
    mnd_span_t content;

    content.start = 0;
    content.end = strlen(text);

    mnd_yaml_test_alloc_init(&ta, 1);
    NAOS_CHECK_FALSE(mnd_yaml_lines_build(text, strlen(text), content, &ta.allocator, &lines));
    NAOS_CHECK_UINT_EQ(1, ta.failures);
    NAOS_CHECK_UINT_EQ(0, lines.count);
    NAOS_CHECK_TRUE(lines.items == NULL);
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));
    mnd_yaml_lines_free(&ta.allocator, &lines);
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));

    mnd_yaml_test_alloc_init(&ta, 0);
    NAOS_REQUIRE_TRUE(mnd_yaml_lines_build(text, strlen(text), content, &ta.allocator, &lines));
    NAOS_CHECK_UINT_EQ(3, lines.count);
    NAOS_CHECK_UINT_EQ(1, mnd_yaml_test_alloc_live(&ta));
    mnd_yaml_lines_free(&ta.allocator, &lines);
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));
}

/* 行の添字と行番号は mnd_source_line_of と同じ数え方（CRLF の CR と LF の間は CR の行） */
NAOS_TEST(lines_index_of_and_line_of_match_source_line_of)
{
    const char *text = "x\na\nbb\r\nc\n";
    mnd_yaml_lines_t lines;
    size_t offset;
    size_t expected = 0;

    NAOS_REQUIRE_TRUE(build_range(text, 2, strlen(text), &lines));
    NAOS_REQUIRE_UINT_EQ(3, lines.count);
    for (offset = 2; offset <= strlen(text); ++offset)
    {
        NAOS_REQUIRE_TRUE(mnd_source_line_of(text, strlen(text), offset, &expected));
        NAOS_CHECK_UINT_EQ(expected, mnd_yaml_lines_line_of(&lines, offset));
    }
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_lines_index_of(&lines, 2));
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_lines_index_of(&lines, 3));
    NAOS_CHECK_UINT_EQ(1, mnd_yaml_lines_index_of(&lines, 4));
    NAOS_CHECK_UINT_EQ(1, mnd_yaml_lines_index_of(&lines, 7));
    NAOS_CHECK_UINT_EQ(2, mnd_yaml_lines_index_of(&lines, 8));
    NAOS_CHECK_UINT_EQ(2, mnd_yaml_lines_index_of(&lines, strlen(text)));
    NAOS_CHECK_UINT_EQ(3, mnd_yaml_lines_line_of(&lines, 7));
    NAOS_CHECK_UINT_EQ(5, mnd_yaml_lines_line_of(&lines, strlen(text)));
    free_lines(&lines);
}

NAOS_SUITE(lines,
    NAOS_ADD_TEST(lines_first_line_number_of_mid_document_range),
    NAOS_ADD_TEST(lines_indent_counts_and_last_line_without_eol),
    NAOS_ADD_TEST(lines_blank_and_comment_detection),
    NAOS_ADD_TEST(lines_tab_in_indent_flag),
    NAOS_ADD_TEST(lines_crlf_eol_len_two_without_cr_in_content),
    NAOS_ADD_TEST(lines_lone_cr_does_not_split_lines),
    NAOS_ADD_TEST(lines_empty_range_and_trailing_eol),
    NAOS_ADD_TEST(lines_allocation_failure_returns_false),
    NAOS_ADD_TEST(lines_index_of_and_line_of_match_source_line_of)
)
