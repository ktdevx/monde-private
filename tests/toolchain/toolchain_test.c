/* ビルド構成の検証: MSVC の /utf-8 と /Zc:preprocessor が有効であること。このファイルは UTF-8 で保存する */
#include <naos/naos.h>

#include <stddef.h>

/* MSVC（clang-cl を除く）では標準準拠のプリプロセッサを必須とする。古い MSVC は _MSVC_TRADITIONAL を定義せず従来型である */
#if defined(_MSC_VER) && !defined(__clang__)
#if !defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL
#error "the standard-conforming preprocessor is required: compile with /Zc:preprocessor"
#endif
#endif

NAOS_TEST(utf8_literal_bytes)
{
    static const char literal[] = "日本";
    static const unsigned char expected[] = { 0xE6, 0x97, 0xA5, 0xE6, 0x9C, 0xAC };
    const unsigned char *bytes = (const unsigned char *)literal;
    size_t i;

    NAOS_REQUIRE_UINT_EQ(sizeof expected, sizeof literal - 1);
    for (i = 0; i < sizeof expected; ++i)
    {
        NAOS_CHECK_UINT_EQ(expected[i], bytes[i]);
    }
}

/* 実行文字セットの検証。/utf-8 がないと ANSI コードページで符号化され、ソースと実行の文字セットが同じため上のテストだけでは検出できないことがある */
NAOS_TEST(universal_character_name_bytes)
{
    static const char literal[] = "\u65E5\u672C";
    static const unsigned char expected[] = { 0xE6, 0x97, 0xA5, 0xE6, 0x9C, 0xAC };
    const unsigned char *bytes = (const unsigned char *)literal;
    size_t i;

    NAOS_REQUIRE_UINT_EQ(sizeof expected, sizeof literal - 1);
    for (i = 0; i < sizeof expected; ++i)
    {
        NAOS_CHECK_UINT_EQ(expected[i], bytes[i]);
    }
}

NAOS_SUITE(source_charset,
    NAOS_ADD_TEST(utf8_literal_bytes),
    NAOS_ADD_TEST(universal_character_name_bytes)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(source_charset)
)
