#include <naos/naos.h>

#include <stdlib.h>
#include <wchar.h>

#include "foundation/wide.h"

/* エスケープの書き換えを避けるため、バイト列とコード単位は数値の配列で書く */
#define AS_CHARS(bytes) ((const char *)(bytes))

/* 失敗時に書き込まれたことを確かめるため、どの理由とも異なる値で初期化する */
static const char wide_test_unset[] = "unset";

/* 「日本」（U+65E5 U+672C）の UTF-8 と UTF-16。どちらも NUL 終端を含む */
static const unsigned char nihon_utf8[] = {0xE6, 0x97, 0xA5, 0xE6, 0x9C, 0xAC, 0x00};
static const wchar_t nihon_utf16[] = {0x65E5, 0x672C, 0x0000};

/* U+1F600 の UTF-8 と UTF-16（サロゲートペア）。どちらも NUL 終端を含む */
static const unsigned char grinning_utf8[] = {0xF0, 0x9F, 0x98, 0x80, 0x00};
static const wchar_t grinning_utf16[] = {0xD83D, 0xDE00, 0x0000};

NAOS_TEST(utf8_to_utf16_converts_japanese)
{
    const char *reason = wide_test_unset;
    wchar_t *wide;

    wide = mnd_wide_from_utf8(AS_CHARS(nihon_utf8), &reason);
    NAOS_REQUIRE_NOT_NULL(wide);
    NAOS_CHECK_MEM_EQ(wide, sizeof nihon_utf16, nihon_utf16, sizeof nihon_utf16);
    free(wide);
}

NAOS_TEST(utf16_to_utf8_converts_japanese)
{
    const char *reason = wide_test_unset;
    char *utf8;

    utf8 = mnd_wide_to_utf8(nihon_utf16, &reason);
    NAOS_REQUIRE_NOT_NULL(utf8);
    NAOS_CHECK_MEM_EQ(utf8, sizeof nihon_utf8, nihon_utf8, sizeof nihon_utf8);
    free(utf8);
}

NAOS_TEST(surrogate_pair_round_trips)
{
    const char *reason = wide_test_unset;
    wchar_t *wide;
    char *utf8;

    wide = mnd_wide_from_utf8(AS_CHARS(grinning_utf8), &reason);
    NAOS_REQUIRE_NOT_NULL(wide);
    NAOS_CHECK_MEM_EQ(wide, sizeof grinning_utf16, grinning_utf16, sizeof grinning_utf16);

    utf8 = mnd_wide_to_utf8(wide, &reason);
    free(wide);
    NAOS_REQUIRE_NOT_NULL(utf8);
    NAOS_CHECK_MEM_EQ(utf8, sizeof grinning_utf8, grinning_utf8, sizeof grinning_utf8);
    free(utf8);
}

NAOS_TEST(empty_string_converts_to_empty_string)
{
    static const wchar_t empty_wide[] = {0x0000};
    const char *reason = wide_test_unset;
    wchar_t *wide;
    char *utf8;

    wide = mnd_wide_from_utf8("", &reason);
    NAOS_REQUIRE_NOT_NULL(wide);
    NAOS_CHECK_UINT_EQ(0x0000, (unsigned)wide[0]);
    free(wide);

    utf8 = mnd_wide_to_utf8(empty_wide, &reason);
    NAOS_REQUIRE_NOT_NULL(utf8);
    NAOS_CHECK_STR_EQ("", utf8);
    free(utf8);
}

NAOS_TEST(invalid_utf8_fails_with_reason)
{
    /* U+0000 の 2 バイトの冗長な表現 */
    static const unsigned char overlong[] = {0xC0, 0x80, 0x00};
    const char *reason = wide_test_unset;
    wchar_t *wide;

    wide = mnd_wide_from_utf8(AS_CHARS(overlong), &reason);
    NAOS_CHECK_NULL(wide);
    NAOS_CHECK_STR_EQ("invalid UTF-8", reason);
    free(wide);
}

NAOS_TEST(lone_surrogate_fails_with_reason)
{
    static const wchar_t lone[] = {0xD800, 0x0000};
    const char *reason = wide_test_unset;
    char *utf8;

    utf8 = mnd_wide_to_utf8(lone, &reason);
    NAOS_CHECK_NULL(utf8);
    NAOS_CHECK_STR_EQ("invalid UTF-16", reason);
    free(utf8);
}

NAOS_SUITE(wide,
    NAOS_ADD_TEST(utf8_to_utf16_converts_japanese),
    NAOS_ADD_TEST(utf16_to_utf8_converts_japanese),
    NAOS_ADD_TEST(surrogate_pair_round_trips),
    NAOS_ADD_TEST(empty_string_converts_to_empty_string),
    NAOS_ADD_TEST(invalid_utf8_fails_with_reason),
    NAOS_ADD_TEST(lone_surrogate_fails_with_reason)
)
