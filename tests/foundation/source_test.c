#include <naos/naos.h>

#include <stddef.h>

#include "foundation/source.h"

/* 文字列リテラルの内容（終端の NUL を除く）の offset の行番号を求める */
#define LINE_OF(literal, offset, line) \
    mnd_source_line_of((literal), sizeof(literal) - 1, (offset), (line))

NAOS_TEST(first_offset_is_line_one)
{
    static const char text[] = "abc\ndef";
    size_t line = 0;

    NAOS_REQUIRE_TRUE(LINE_OF(text, 0, &line));
    NAOS_CHECK_UINT_EQ(1, line);
    NAOS_REQUIRE_TRUE(LINE_OF(text, 3, &line));
    NAOS_CHECK_UINT_EQ(1, line);
}

NAOS_TEST(empty_content_offset_zero_is_line_one)
{
    size_t line = 0;

    NAOS_REQUIRE_TRUE(mnd_source_line_of("", 0, 0, &line));
    NAOS_CHECK_UINT_EQ(1, line);
}

NAOS_TEST(line_advances_after_lf)
{
    static const char text[] = "a\nb\nc";
    size_t line = 0;

    /* offset 2 は 1 つ目の LF の直後 */
    NAOS_REQUIRE_TRUE(LINE_OF(text, 2, &line));
    NAOS_CHECK_UINT_EQ(2, line);
    NAOS_REQUIRE_TRUE(LINE_OF(text, 4, &line));
    NAOS_CHECK_UINT_EQ(3, line);
}

NAOS_TEST(line_advances_after_crlf)
{
    static const char text[] = "a\r\nb\r\nc";
    size_t line = 0;

    /* offset 3 は 1 つ目の CRLF の直後 */
    NAOS_REQUIRE_TRUE(LINE_OF(text, 3, &line));
    NAOS_CHECK_UINT_EQ(2, line);
    NAOS_REQUIRE_TRUE(LINE_OF(text, 6, &line));
    NAOS_CHECK_UINT_EQ(3, line);
}

NAOS_TEST(lone_cr_does_not_advance_line)
{
    static const char text[] = "a\rb\rc";
    size_t line = 0;

    NAOS_REQUIRE_TRUE(LINE_OF(text, 2, &line));
    NAOS_CHECK_UINT_EQ(1, line);
    NAOS_REQUIRE_TRUE(LINE_OF(text, 5, &line));
    NAOS_CHECK_UINT_EQ(1, line);
}

NAOS_TEST(between_cr_and_lf_is_on_cr_line)
{
    static const char text[] = "a\nb\r\nc";
    size_t line = 0;

    /* offset 3 は 2 行目の CR、offset 4 は CR と LF の間（LF の位置） */
    NAOS_REQUIRE_TRUE(LINE_OF(text, 3, &line));
    NAOS_CHECK_UINT_EQ(2, line);
    NAOS_REQUIRE_TRUE(LINE_OF(text, 4, &line));
    NAOS_CHECK_UINT_EQ(2, line);
    NAOS_REQUIRE_TRUE(LINE_OF(text, 5, &line));
    NAOS_CHECK_UINT_EQ(3, line);
}

NAOS_TEST(offset_equal_to_len_is_valid)
{
    static const char text[] = "a\nb\n";
    size_t line = 0;

    NAOS_REQUIRE_TRUE(LINE_OF(text, sizeof(text) - 1, &line));
    NAOS_CHECK_UINT_EQ(3, line);
}

NAOS_TEST(offset_past_len_fails_without_touching_line)
{
    static const char text[] = "a\nb\n";
    size_t line = 42;

    NAOS_CHECK_FALSE(LINE_OF(text, sizeof(text), &line));
    NAOS_CHECK_UINT_EQ(42, line);
}

NAOS_TEST(valid_spans)
{
    mnd_span_t whole = { 0, 5 };
    mnd_span_t empty_at_end = { 5, 5 };
    mnd_span_t empty_at_start = { 0, 0 };
    mnd_span_t inner = { 1, 3 };

    NAOS_CHECK_TRUE(mnd_span_is_valid(whole, 5));
    NAOS_CHECK_TRUE(mnd_span_is_valid(empty_at_end, 5));
    NAOS_CHECK_TRUE(mnd_span_is_valid(empty_at_start, 0));
    NAOS_CHECK_TRUE(mnd_span_is_valid(inner, 5));
}

NAOS_TEST(start_after_end_is_invalid)
{
    mnd_span_t span = { 3, 2 };

    NAOS_CHECK_FALSE(mnd_span_is_valid(span, 5));
}

NAOS_TEST(end_past_len_is_invalid)
{
    mnd_span_t span = { 2, 6 };
    mnd_span_t both_past = { 6, 6 };

    NAOS_CHECK_FALSE(mnd_span_is_valid(span, 5));
    NAOS_CHECK_FALSE(mnd_span_is_valid(both_past, 5));
}

NAOS_TEST(span_text_points_into_original)
{
    static const char text[] = "hello\r\nworld";
    mnd_span_t span = { 3, 9 };
    mnd_slice_t out = { NULL, 0 };

    NAOS_REQUIRE_TRUE(mnd_span_text(text, sizeof(text) - 1, span, &out));
    NAOS_CHECK_PTR_EQ(text + 3, out.data);
    NAOS_CHECK_UINT_EQ(6, out.len);
    NAOS_CHECK_MEM_EQ("lo\r\nwo", 6, out.data, out.len);
}

NAOS_TEST(empty_span_text_at_end)
{
    static const char text[] = "abc";
    mnd_span_t span = { 3, 3 };
    mnd_slice_t out = { NULL, 99 };

    NAOS_REQUIRE_TRUE(mnd_span_text(text, sizeof(text) - 1, span, &out));
    NAOS_CHECK_PTR_EQ(text + 3, out.data);
    NAOS_CHECK_UINT_EQ(0, out.len);
}

NAOS_TEST(invalid_span_text_fails_without_touching_out)
{
    static const char text[] = "abc";
    mnd_span_t reversed = { 2, 1 };
    mnd_span_t past_end = { 1, 4 };
    mnd_slice_t out = { "sentinel", 8 };
    const char *before = out.data;

    NAOS_CHECK_FALSE(mnd_span_text(text, sizeof(text) - 1, reversed, &out));
    NAOS_CHECK_PTR_EQ(before, out.data);
    NAOS_CHECK_UINT_EQ(8, out.len);
    NAOS_CHECK_FALSE(mnd_span_text(text, sizeof(text) - 1, past_end, &out));
    NAOS_CHECK_PTR_EQ(before, out.data);
    NAOS_CHECK_UINT_EQ(8, out.len);
}

NAOS_SUITE(source,
    NAOS_ADD_TEST(first_offset_is_line_one),
    NAOS_ADD_TEST(empty_content_offset_zero_is_line_one),
    NAOS_ADD_TEST(line_advances_after_lf),
    NAOS_ADD_TEST(line_advances_after_crlf),
    NAOS_ADD_TEST(lone_cr_does_not_advance_line),
    NAOS_ADD_TEST(between_cr_and_lf_is_on_cr_line),
    NAOS_ADD_TEST(offset_equal_to_len_is_valid),
    NAOS_ADD_TEST(offset_past_len_fails_without_touching_line),
    NAOS_ADD_TEST(valid_spans),
    NAOS_ADD_TEST(start_after_end_is_invalid),
    NAOS_ADD_TEST(end_past_len_is_invalid),
    NAOS_ADD_TEST(span_text_points_into_original),
    NAOS_ADD_TEST(empty_span_text_at_end),
    NAOS_ADD_TEST(invalid_span_text_fails_without_touching_out)
)
