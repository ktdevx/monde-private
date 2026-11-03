#include <naos/naos.h>

#include <stddef.h>

#include "foundation/eol.h"

/* 文字列リテラルの内容（終端の NUL を除く）を検出する */
#define DETECT(literal) mnd_eol_detect((literal), sizeof(literal) - 1)

NAOS_TEST(lf_only_is_lf)
{
    mnd_eol_info_t info = DETECT("a\nb\nc\n");

    NAOS_CHECK_INT_EQ(MND_EOL_LF, info.dominant);
    NAOS_CHECK_FALSE(info.mixed);
    NAOS_CHECK_UINT_EQ(3, info.lf_count);
    NAOS_CHECK_UINT_EQ(0, info.crlf_count);
}

NAOS_TEST(crlf_only_is_crlf)
{
    mnd_eol_info_t info = DETECT("a\r\nb\r\n");

    NAOS_CHECK_INT_EQ(MND_EOL_CRLF, info.dominant);
    NAOS_CHECK_FALSE(info.mixed);
    NAOS_CHECK_UINT_EQ(0, info.lf_count);
    NAOS_CHECK_UINT_EQ(2, info.crlf_count);
}

NAOS_TEST(no_line_break_is_none)
{
    mnd_eol_info_t info = DETECT("abc");

    NAOS_CHECK_INT_EQ(MND_EOL_NONE, info.dominant);
    NAOS_CHECK_FALSE(info.mixed);
    NAOS_CHECK_UINT_EQ(0, info.lf_count);
    NAOS_CHECK_UINT_EQ(0, info.crlf_count);
}

NAOS_TEST(empty_input_is_none)
{
    mnd_eol_info_t info = mnd_eol_detect(NULL, 0);

    NAOS_CHECK_INT_EQ(MND_EOL_NONE, info.dominant);
    NAOS_CHECK_FALSE(info.mixed);
    NAOS_CHECK_UINT_EQ(0, info.lf_count);
    NAOS_CHECK_UINT_EQ(0, info.crlf_count);

    info = DETECT("");
    NAOS_CHECK_INT_EQ(MND_EOL_NONE, info.dominant);
    NAOS_CHECK_FALSE(info.mixed);
}

NAOS_TEST(majority_lf_is_lf_and_mixed)
{
    /* LF 2 + CRLF 1。CRLF が先に現れても多い方が勝つ */
    mnd_eol_info_t info = DETECT("a\r\nb\nc\n");

    NAOS_CHECK_INT_EQ(MND_EOL_LF, info.dominant);
    NAOS_CHECK_TRUE(info.mixed);
    NAOS_CHECK_UINT_EQ(2, info.lf_count);
    NAOS_CHECK_UINT_EQ(1, info.crlf_count);
}

NAOS_TEST(majority_crlf_is_crlf_and_mixed)
{
    /* CRLF 2 + LF 1。LF が先に現れても多い方が勝つ */
    mnd_eol_info_t info = DETECT("a\nb\r\nc\r\n");

    NAOS_CHECK_INT_EQ(MND_EOL_CRLF, info.dominant);
    NAOS_CHECK_TRUE(info.mixed);
    NAOS_CHECK_UINT_EQ(1, info.lf_count);
    NAOS_CHECK_UINT_EQ(2, info.crlf_count);
}

NAOS_TEST(tie_with_crlf_first_is_crlf)
{
    mnd_eol_info_t info = DETECT("\r\n\n");

    NAOS_CHECK_INT_EQ(MND_EOL_CRLF, info.dominant);
    NAOS_CHECK_TRUE(info.mixed);
    NAOS_CHECK_UINT_EQ(1, info.lf_count);
    NAOS_CHECK_UINT_EQ(1, info.crlf_count);
}

NAOS_TEST(tie_with_lf_first_is_lf)
{
    mnd_eol_info_t info = DETECT("\n\r\n");

    NAOS_CHECK_INT_EQ(MND_EOL_LF, info.dominant);
    NAOS_CHECK_TRUE(info.mixed);
    NAOS_CHECK_UINT_EQ(1, info.lf_count);
    NAOS_CHECK_UINT_EQ(1, info.crlf_count);
}

NAOS_TEST(lone_cr_is_not_line_break)
{
    mnd_eol_info_t info = DETECT("a\rb");

    NAOS_CHECK_INT_EQ(MND_EOL_NONE, info.dominant);
    NAOS_CHECK_FALSE(info.mixed);
    NAOS_CHECK_UINT_EQ(0, info.lf_count);
    NAOS_CHECK_UINT_EQ(0, info.crlf_count);
}

NAOS_TEST(cr_at_end_is_not_line_break)
{
    mnd_eol_info_t info = DETECT("a\r");

    NAOS_CHECK_INT_EQ(MND_EOL_NONE, info.dominant);
    NAOS_CHECK_UINT_EQ(0, info.lf_count);
    NAOS_CHECK_UINT_EQ(0, info.crlf_count);

    /* 末尾の CR は直前の LF の数え方に影響しない */
    info = DETECT("a\nb\r");
    NAOS_CHECK_INT_EQ(MND_EOL_LF, info.dominant);
    NAOS_CHECK_FALSE(info.mixed);
    NAOS_CHECK_UINT_EQ(1, info.lf_count);
    NAOS_CHECK_UINT_EQ(0, info.crlf_count);
}

NAOS_TEST(cr_cr_lf_is_one_crlf)
{
    mnd_eol_info_t info = DETECT("\r\r\n");

    NAOS_CHECK_INT_EQ(MND_EOL_CRLF, info.dominant);
    NAOS_CHECK_FALSE(info.mixed);
    NAOS_CHECK_UINT_EQ(0, info.lf_count);
    NAOS_CHECK_UINT_EQ(1, info.crlf_count);
}

NAOS_TEST(lf_cr_is_one_lf)
{
    /* LF の後の CR は次の CRLF の始まりにならない */
    mnd_eol_info_t info = DETECT("a\n\rb");

    NAOS_CHECK_INT_EQ(MND_EOL_LF, info.dominant);
    NAOS_CHECK_FALSE(info.mixed);
    NAOS_CHECK_UINT_EQ(1, info.lf_count);
    NAOS_CHECK_UINT_EQ(0, info.crlf_count);
}

NAOS_TEST(length_limits_the_scan)
{
    /* 長さの外にある LF は数えず、長さの末尾で切れた CR は改行ではない */
    static const char text[] = "a\r\nb\n";
    mnd_eol_info_t info = mnd_eol_detect(text, 2);

    NAOS_CHECK_INT_EQ(MND_EOL_NONE, info.dominant);
    NAOS_CHECK_UINT_EQ(0, info.lf_count);
    NAOS_CHECK_UINT_EQ(0, info.crlf_count);

    info = mnd_eol_detect(text, 3);
    NAOS_CHECK_INT_EQ(MND_EOL_CRLF, info.dominant);
    NAOS_CHECK_FALSE(info.mixed);
    NAOS_CHECK_UINT_EQ(1, info.crlf_count);
}

NAOS_TEST(nul_bytes_are_ordinary_content)
{
    static const char text[] = {'\0', '\n', '\0', '\r', '\n', '\0', '\n'};
    mnd_eol_info_t info = mnd_eol_detect(text, sizeof(text));

    NAOS_CHECK_INT_EQ(MND_EOL_LF, info.dominant);
    NAOS_CHECK_TRUE(info.mixed);
    NAOS_CHECK_UINT_EQ(2, info.lf_count);
    NAOS_CHECK_UINT_EQ(1, info.crlf_count);
}

NAOS_SUITE(eol,
    NAOS_ADD_TEST(lf_only_is_lf),
    NAOS_ADD_TEST(crlf_only_is_crlf),
    NAOS_ADD_TEST(no_line_break_is_none),
    NAOS_ADD_TEST(empty_input_is_none),
    NAOS_ADD_TEST(majority_lf_is_lf_and_mixed),
    NAOS_ADD_TEST(majority_crlf_is_crlf_and_mixed),
    NAOS_ADD_TEST(tie_with_crlf_first_is_crlf),
    NAOS_ADD_TEST(tie_with_lf_first_is_lf),
    NAOS_ADD_TEST(lone_cr_is_not_line_break),
    NAOS_ADD_TEST(cr_at_end_is_not_line_break),
    NAOS_ADD_TEST(cr_cr_lf_is_one_crlf),
    NAOS_ADD_TEST(lf_cr_is_one_lf),
    NAOS_ADD_TEST(length_limits_the_scan),
    NAOS_ADD_TEST(nul_bytes_are_ordinary_content)
)
