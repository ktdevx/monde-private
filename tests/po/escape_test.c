/* Escape: 文字列リテラルのエスケープとアンエスケープ */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "po/po_internal.h"

#include <string.h>

/* テスト用: NUL 終端の lit をアンエスケープし、成功したかを返す */
static bool unescape_ok(const char *lit, mnd_buf_t *out)
{
    size_t bad_offset = 0;

    return mnd_po_unescape(lit, strlen(lit), out, &bad_offset) == MND_PO_UNESCAPE_OK;
}

NAOS_TEST(escape_unescape_status_ok_is_distinct)
{
    NAOS_CHECK_INT_NE(MND_PO_UNESCAPE_OK, MND_PO_UNESCAPE_BAD_ESCAPE);
    NAOS_CHECK_INT_NE(MND_PO_UNESCAPE_OK, MND_PO_UNESCAPE_NUL);
    NAOS_CHECK_INT_NE(MND_PO_UNESCAPE_OK, MND_PO_UNESCAPE_BAD_UTF8);
    NAOS_CHECK_INT_NE(MND_PO_UNESCAPE_OK, MND_PO_UNESCAPE_NO_MEMORY);
}

NAOS_TEST(escape_unescape_simple_escapes)
{
    mnd_buf_t out = MND_BUF_INIT;
    const char *lit = "\\\\\\\"\\n\\t\\r\\a\\b\\f\\v";
    const char expected[] = "\\\"\n\t\r\a\b\f\v";

    NAOS_CHECK_TRUE(unescape_ok(lit, &out));
    NAOS_CHECK_MEM_EQ(out.data, out.len, expected, sizeof(expected) - 1);
    mnd_buf_free(&out);
}

NAOS_TEST(escape_unescape_keeps_plain_text_and_appends)
{
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "pre:"));
    NAOS_CHECK_TRUE(unescape_ok("a\\nb \xE3\x81\x82", &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "pre:a\nb \xE3\x81\x82");
    mnd_buf_free(&out);
}

NAOS_TEST(escape_unescape_empty)
{
    mnd_buf_t out = MND_BUF_INIT;
    size_t bad_offset = 0;

    NAOS_CHECK_INT_EQ(mnd_po_unescape("", 0, &out, &bad_offset), MND_PO_UNESCAPE_OK);
    NAOS_CHECK_UINT_EQ(out.len, (size_t)0);
    mnd_buf_free(&out);
}

NAOS_TEST(escape_unescape_octal_and_hex)
{
    mnd_buf_t out = MND_BUF_INIT;

    NAOS_CHECK_TRUE(unescape_ok("\\101\\x41", &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "AA");
    mnd_buf_clear(&out);

    /* 8 進数は 1〜3 桁。4 桁目は通常の文字 */
    NAOS_CHECK_TRUE(unescape_ok("\\1\\12\\1012", &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "\001\012A2");
    mnd_buf_clear(&out);

    /* 16 進数は 1 桁以上。先頭の 0 は値を変えない。大文字と小文字 */
    NAOS_CHECK_TRUE(unescape_ok("\\x7\\x0041\\x7f\\x7F", &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "\007A\177\177");
    mnd_buf_clear(&out);

    /* 8 進数と 16 進数で UTF-8 のバイト列を作る（あ = E3 81 82） */
    NAOS_CHECK_TRUE(unescape_ok("\\343\\x81\\202", &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "\xE3\x81\x82");
    mnd_buf_free(&out);
}

/* lit のアンエスケープが BAD_ESCAPE で、位置が expected_offset、out が変わらないことを確かめる */
static void check_bad_escape(const char *lit, size_t expected_offset)
{
    mnd_buf_t out = MND_BUF_INIT;
    size_t bad_offset = 999;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "keep"));
    NAOS_CHECK_INT_EQ(mnd_po_unescape(lit, strlen(lit), &out, &bad_offset),
                      MND_PO_UNESCAPE_BAD_ESCAPE);
    NAOS_CHECK_UINT_EQ(bad_offset, expected_offset);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "keep");
    mnd_buf_free(&out);
}

NAOS_TEST(escape_unescape_hex_over_255_is_bad_escape)
{
    check_bad_escape("\\x1ff", 0);
    check_bad_escape("ab\\x100", 2);
    /* 桁が多くても値は 255 を超える */
    check_bad_escape("\\x123456789abcdef0123456789", 0);
}

NAOS_TEST(escape_unescape_octal_over_255_is_bad_escape)
{
    check_bad_escape("\\400", 0);
    check_bad_escape("x\\777", 1);
}

NAOS_TEST(escape_unescape_unknown_escape_is_bad_escape)
{
    check_bad_escape("\\q", 0);
    check_bad_escape("abc\\q", 3);
    check_bad_escape("\\n\\8", 2);
    check_bad_escape("\\'", 0);
    check_bad_escape("\\?", 0);
    check_bad_escape("\\e", 0);
    check_bad_escape("\\u3042", 0);
    /* 数字のない \x */
    check_bad_escape("\\x", 0);
    check_bad_escape("\\xg", 0);
    /* 末尾の \ */
    check_bad_escape("a\\", 1);
    /* 最初の誤りの位置を返す */
    check_bad_escape("\\n\\q\\z", 2);
}

NAOS_TEST(escape_unescape_nul_is_error)
{
    mnd_buf_t out = MND_BUF_INIT;
    size_t bad_offset = 0;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "keep"));
    NAOS_CHECK_INT_EQ(mnd_po_unescape("\\0", 2, &out, &bad_offset), MND_PO_UNESCAPE_NUL);
    NAOS_CHECK_INT_EQ(mnd_po_unescape("a\\000b", 6, &out, &bad_offset), MND_PO_UNESCAPE_NUL);
    NAOS_CHECK_INT_EQ(mnd_po_unescape("\\x0", 3, &out, &bad_offset), MND_PO_UNESCAPE_NUL);
    /* リテラルにそのまま含まれる NUL も誤り */
    NAOS_CHECK_INT_EQ(mnd_po_unescape("a\0b", 3, &out, &bad_offset), MND_PO_UNESCAPE_NUL);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "keep");
    NAOS_CHECK_UINT_EQ(out.len, (size_t)4);
    mnd_buf_free(&out);
}

NAOS_TEST(escape_unescape_invalid_utf8_is_error)
{
    mnd_buf_t out = MND_BUF_INIT;
    size_t bad_offset = 0;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "keep"));
    NAOS_CHECK_INT_EQ(mnd_po_unescape("\\xff", 4, &out, &bad_offset),
                      MND_PO_UNESCAPE_BAD_UTF8);
    /* 途中で切れたシーケンス */
    NAOS_CHECK_INT_EQ(mnd_po_unescape("\\343\\201", 8, &out, &bad_offset),
                      MND_PO_UNESCAPE_BAD_UTF8);
    /* 単独の継続バイト */
    NAOS_CHECK_INT_EQ(mnd_po_unescape("a\\x80", 5, &out, &bad_offset),
                      MND_PO_UNESCAPE_BAD_UTF8);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "keep");
    NAOS_CHECK_UINT_EQ(out.len, (size_t)4);
    mnd_buf_free(&out);
}

NAOS_TEST(escape_escape_each_special_character)
{
    mnd_buf_t out = MND_BUF_INIT;
    const char s[] = "\\\"\n\t\r\a\b\f\v";

    NAOS_CHECK_TRUE(mnd_po_escape(s, sizeof(s) - 1, &out));
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), "\\\\\\\"\\n\\t\\r\\a\\b\\f\\v");
    mnd_buf_free(&out);
}

NAOS_TEST(escape_escape_writes_other_characters_as_is)
{
    mnd_buf_t out = MND_BUF_INIT;
    /* ほかの制御文字（0x01、ESC、DEL）、'、?、非 ASCII（日本語、絵文字）はそのまま */
    const char s[] = "\001\033\177'? \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80";

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out, "pre:"));
    NAOS_CHECK_TRUE(mnd_po_escape(s, sizeof(s) - 1, &out));
    NAOS_CHECK_UINT_EQ(out.len, (size_t)4 + (sizeof(s) - 1));
    NAOS_CHECK_MEM_EQ(out.data + 4, out.len - 4, s, sizeof(s) - 1);
    mnd_buf_clear(&out);

    NAOS_CHECK_TRUE(mnd_po_escape("", 0, &out));
    NAOS_CHECK_UINT_EQ(out.len, (size_t)0);
    mnd_buf_free(&out);
}

/* s[0..len) をエスケープしてからアンエスケープした結果が s と一致することを確かめる */
static void check_round_trip(const char *s, size_t len)
{
    mnd_buf_t escaped = MND_BUF_INIT;
    mnd_buf_t restored = MND_BUF_INIT;
    size_t bad_offset = 0;

    NAOS_REQUIRE_TRUE(mnd_po_escape(s, len, &escaped));
    NAOS_CHECK_INT_EQ(mnd_po_unescape(escaped.data, escaped.len, &restored, &bad_offset),
                      MND_PO_UNESCAPE_OK);
    NAOS_CHECK_MEM_EQ(restored.data, restored.len, s, len);
    mnd_buf_free(&escaped);
    mnd_buf_free(&restored);
}

NAOS_TEST(escape_round_trip_all_ascii)
{
    char s[127];
    size_t i;

    for (i = 0; i < sizeof(s); i++)
    {
        s[i] = (char)(i + 1);
    }
    check_round_trip(s, sizeof(s));
}

NAOS_TEST(escape_round_trip_japanese_and_emoji)
{
    /* 「日本語の"テキスト"\n」、絵文字（😀、👍🏽、🇯🇵、👨‍👩‍👧）、タブと \ */
    const char s[] = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x81\xAE\"\xE3\x83\x86\xE3\x82\xAD"
                     "\xE3\x82\xB9\xE3\x83\x88\"\n"
                     "\xF0\x9F\x98\x80 \xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD "
                     "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5 "
                     "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7"
                     "\t\\end";

    check_round_trip(s, sizeof(s) - 1);
}

NAOS_SUITE(escape,
    NAOS_ADD_TEST(escape_unescape_status_ok_is_distinct),
    NAOS_ADD_TEST(escape_unescape_simple_escapes),
    NAOS_ADD_TEST(escape_unescape_keeps_plain_text_and_appends),
    NAOS_ADD_TEST(escape_unescape_empty),
    NAOS_ADD_TEST(escape_unescape_octal_and_hex),
    NAOS_ADD_TEST(escape_unescape_hex_over_255_is_bad_escape),
    NAOS_ADD_TEST(escape_unescape_octal_over_255_is_bad_escape),
    NAOS_ADD_TEST(escape_unescape_unknown_escape_is_bad_escape),
    NAOS_ADD_TEST(escape_unescape_nul_is_error),
    NAOS_ADD_TEST(escape_unescape_invalid_utf8_is_error),
    NAOS_ADD_TEST(escape_escape_each_special_character),
    NAOS_ADD_TEST(escape_escape_writes_other_characters_as_is),
    NAOS_ADD_TEST(escape_round_trip_all_ascii),
    NAOS_ADD_TEST(escape_round_trip_japanese_and_emoji)
)
