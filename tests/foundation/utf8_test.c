#include <naos/naos.h>

#include <stddef.h>

#include "foundation/utf8.h"

/* エスケープの書き換えを避けるため、バイト列は数値の配列で書く */
#define AS_CHARS(bytes) ((const char *)(bytes))

/* 不正と判定されたときに書き込まれたことを確かめるため、範囲外の値で初期化する */
#define UNSET_OFFSET ((size_t)-1)

NAOS_TEST(empty_input_is_valid)
{
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_TRUE(mnd_utf8_validate(NULL, 0, &offset));
    NAOS_CHECK_TRUE(mnd_utf8_validate("", 0, &offset));
}

NAOS_TEST(ascii_is_valid)
{
    /* U+007F を含む */
    static const unsigned char bytes[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x0A, 0x7F};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_TRUE(mnd_utf8_validate(AS_CHARS(bytes), sizeof(bytes), &offset));
}

NAOS_TEST(nul_is_valid)
{
    static const unsigned char bytes[] = {0x61, 0x00, 0x62, 0x00};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_TRUE(mnd_utf8_validate(AS_CHARS(bytes), sizeof(bytes), &offset));
}

NAOS_TEST(two_byte_characters_are_valid)
{
    /* U+0080、U+00E9、U+07FF */
    static const unsigned char bytes[] = {0xC2, 0x80, 0xC3, 0xA9, 0xDF, 0xBF};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_TRUE(mnd_utf8_validate(AS_CHARS(bytes), sizeof(bytes), &offset));
}

NAOS_TEST(three_byte_characters_are_valid)
{
    /* U+0800、U+65E5、U+D7FF、U+E000、U+FFFF */
    static const unsigned char bytes[] =
    {
        0xE0, 0xA0, 0x80,
        0xE6, 0x97, 0xA5,
        0xED, 0x9F, 0xBF,
        0xEE, 0x80, 0x80,
        0xEF, 0xBF, 0xBF,
    };
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_TRUE(mnd_utf8_validate(AS_CHARS(bytes), sizeof(bytes), &offset));
}

NAOS_TEST(four_byte_characters_are_valid)
{
    /* U+10000、U+1F600、U+40000 */
    static const unsigned char bytes[] =
    {
        0xF0, 0x90, 0x80, 0x80,
        0xF0, 0x9F, 0x98, 0x80,
        0xF1, 0x80, 0x80, 0x80,
    };
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_TRUE(mnd_utf8_validate(AS_CHARS(bytes), sizeof(bytes), &offset));
}

NAOS_TEST(max_code_point_is_valid)
{
    /* U+10FFFF */
    static const unsigned char bytes[] = {0xF4, 0x8F, 0xBF, 0xBF};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_TRUE(mnd_utf8_validate(AS_CHARS(bytes), sizeof(bytes), &offset));
}

NAOS_TEST(overlong_two_byte_is_invalid)
{
    static const unsigned char c0[] = {0xC0, 0x80};
    static const unsigned char c1[] = {0x41, 0xC1, 0xBF};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(c0), sizeof(c0), &offset));
    NAOS_CHECK_UINT_EQ(0, offset);

    offset = UNSET_OFFSET;
    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(c1), sizeof(c1), &offset));
    NAOS_CHECK_UINT_EQ(1, offset);
}

NAOS_TEST(overlong_three_and_four_byte_are_invalid)
{
    static const unsigned char three[] = {0xE0, 0x80, 0x80};
    static const unsigned char three_max[] = {0xE0, 0x9F, 0xBF};
    static const unsigned char four[] = {0xF0, 0x80, 0x80, 0x80};
    static const unsigned char four_max[] = {0xF0, 0x8F, 0xBF, 0xBF};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(three), sizeof(three), &offset));
    NAOS_CHECK_UINT_EQ(0, offset);

    offset = UNSET_OFFSET;
    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(three_max), sizeof(three_max), &offset));
    NAOS_CHECK_UINT_EQ(0, offset);

    offset = UNSET_OFFSET;
    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(four), sizeof(four), &offset));
    NAOS_CHECK_UINT_EQ(0, offset);

    offset = UNSET_OFFSET;
    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(four_max), sizeof(four_max), &offset));
    NAOS_CHECK_UINT_EQ(0, offset);
}

NAOS_TEST(surrogates_are_invalid)
{
    static const unsigned char first[] = {0x61, 0x62, 0xED, 0xA0, 0x80};
    static const unsigned char last[] = {0xED, 0xBF, 0xBF};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(first), sizeof(first), &offset));
    NAOS_CHECK_UINT_EQ(2, offset);

    offset = UNSET_OFFSET;
    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(last), sizeof(last), &offset));
    NAOS_CHECK_UINT_EQ(0, offset);
}

NAOS_TEST(beyond_max_code_point_is_invalid)
{
    static const unsigned char f4_90[] = {0xF4, 0x90, 0x80, 0x80};
    static const unsigned char f5[] = {0x61, 0xF5, 0x80, 0x80, 0x80};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(f4_90), sizeof(f4_90), &offset));
    NAOS_CHECK_UINT_EQ(0, offset);

    offset = UNSET_OFFSET;
    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(f5), sizeof(f5), &offset));
    NAOS_CHECK_UINT_EQ(1, offset);
}

NAOS_TEST(invalid_lead_bytes_are_invalid)
{
    unsigned int lead;

    for (lead = 0xF5; lead <= 0xFF; ++lead)
    {
        unsigned char bytes[3];
        size_t offset = UNSET_OFFSET;

        bytes[0] = 0x41;
        bytes[1] = 0x42;
        bytes[2] = (unsigned char)lead;
        NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(bytes), sizeof(bytes), &offset));
        NAOS_CHECK_UINT_EQ(2, offset);
    }
}

NAOS_TEST(lone_ff_is_invalid)
{
    static const unsigned char bytes[] = {0xFF};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(bytes), sizeof(bytes), &offset));
    NAOS_CHECK_UINT_EQ(0, offset);
}

NAOS_TEST(lone_continuation_byte_is_invalid)
{
    static const unsigned char lone[] = {0x80};
    static const unsigned char after_char[] = {0xC3, 0xA9, 0xBF, 0x61};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(lone), sizeof(lone), &offset));
    NAOS_CHECK_UINT_EQ(0, offset);

    offset = UNSET_OFFSET;
    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(after_char), sizeof(after_char), &offset));
    NAOS_CHECK_UINT_EQ(2, offset);
}

NAOS_TEST(truncated_sequence_at_end_is_invalid)
{
    /* U+65E5 の最後の 1 バイトが欠けている */
    static const unsigned char three[] = {0x61, 0xE6, 0x97};
    /* U+1F600 の最後の 1 バイトが欠けている */
    static const unsigned char four[] = {0x61, 0x62, 0xF0, 0x9F, 0x98};
    static const unsigned char two[] = {0xC3};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(three), sizeof(three), &offset));
    NAOS_CHECK_UINT_EQ(1, offset);

    offset = UNSET_OFFSET;
    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(four), sizeof(four), &offset));
    NAOS_CHECK_UINT_EQ(2, offset);

    offset = UNSET_OFFSET;
    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(two), sizeof(two), &offset));
    NAOS_CHECK_UINT_EQ(0, offset);
}

NAOS_TEST(truncated_sequence_before_other_byte_is_invalid)
{
    /* 継続バイトの位置に ASCII が来ている */
    static const unsigned char bytes[] = {0x61, 0xE6, 0x97, 0x41, 0x62};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(bytes), sizeof(bytes), &offset));
    NAOS_CHECK_UINT_EQ(1, offset);
}

NAOS_TEST(length_limits_the_checked_range)
{
    /* len の外にあるバイトは検査しない */
    static const unsigned char bytes[] = {0x61, 0xC3, 0xA9, 0xFF};
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_TRUE(mnd_utf8_validate(AS_CHARS(bytes), 3, &offset));

    offset = UNSET_OFFSET;
    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(bytes), 2, &offset));
    NAOS_CHECK_UINT_EQ(1, offset);
}

NAOS_TEST(offset_points_to_first_invalid_sequence)
{
    /* "a"、U+00E9、U+65E5 の後に不正な FF、その後ろにも不正な C0 80 */
    static const unsigned char bytes[] =
    {
        0x61, 0xC3, 0xA9, 0xE6, 0x97, 0xA5, 0xFF, 0x62, 0xC0, 0x80,
    };
    size_t offset = UNSET_OFFSET;

    NAOS_CHECK_FALSE(mnd_utf8_validate(AS_CHARS(bytes), sizeof(bytes), &offset));
    NAOS_CHECK_UINT_EQ(6, offset);
}

NAOS_SUITE(utf8,
    NAOS_ADD_TEST(empty_input_is_valid),
    NAOS_ADD_TEST(ascii_is_valid),
    NAOS_ADD_TEST(nul_is_valid),
    NAOS_ADD_TEST(two_byte_characters_are_valid),
    NAOS_ADD_TEST(three_byte_characters_are_valid),
    NAOS_ADD_TEST(four_byte_characters_are_valid),
    NAOS_ADD_TEST(max_code_point_is_valid),
    NAOS_ADD_TEST(overlong_two_byte_is_invalid),
    NAOS_ADD_TEST(overlong_three_and_four_byte_are_invalid),
    NAOS_ADD_TEST(surrogates_are_invalid),
    NAOS_ADD_TEST(beyond_max_code_point_is_invalid),
    NAOS_ADD_TEST(invalid_lead_bytes_are_invalid),
    NAOS_ADD_TEST(lone_ff_is_invalid),
    NAOS_ADD_TEST(lone_continuation_byte_is_invalid),
    NAOS_ADD_TEST(truncated_sequence_at_end_is_invalid),
    NAOS_ADD_TEST(truncated_sequence_before_other_byte_is_invalid),
    NAOS_ADD_TEST(length_limits_the_checked_range),
    NAOS_ADD_TEST(offset_points_to_first_invalid_sequence)
)
