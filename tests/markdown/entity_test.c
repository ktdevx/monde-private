/* Entity: 実体名の表と検索 */

#include <naos/naos.h>

#include "markdown/md_entity.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* NUL 終端の name を検索し、見つかって表す列が expected（NUL 終端の UTF-8）と一致するかを確かめる */
static void check_found(const char *name, const char *expected)
{
    const unsigned char *utf8 = NULL;
    size_t len = 0;

    NAOS_REQUIRE_TRUE(mnd_md_entity_lookup(name, strlen(name), &utf8, &len));
    NAOS_REQUIRE_NOT_NULL(utf8);
    NAOS_CHECK_MEM_EQ(utf8, len, expected, strlen(expected));
}

/* name[0..len) が見つからず、出力の引数が変わらないことを確かめる */
static void check_not_found(const char *name, size_t len)
{
    static const unsigned char sentinel[1] = { 0 };
    const unsigned char *utf8 = sentinel;
    size_t utf8_len = 12345;

    NAOS_CHECK_FALSE(mnd_md_entity_lookup(name, len, &utf8, &utf8_len));
    NAOS_CHECK_PTR_EQ(sentinel, utf8);
    NAOS_CHECK_UINT_EQ(12345, utf8_len);
}

NAOS_TEST(amp_is_ampersand)
{
    check_found("amp", "&");
    check_found("lt", "<");
    check_found("quot", "\"");
}

NAOS_TEST(ngE_is_two_codepoints)
{
    /* U+2267 U+0338 */
    check_found("ngE", "\xE2\x89\xA7\xCC\xB8");
}

NAOS_TEST(multibyte_and_astral)
{
    check_found("copy", "\xC2\xA9");           /* U+00A9 */
    check_found("nbsp", "\xC2\xA0");           /* U+00A0 */
    check_found("Afr", "\xF0\x9D\x94\x84");    /* U+1D504（4 バイト） */
    check_found("CounterClockwiseContourIntegral", "\xE2\x88\xB3"); /* 最長の名前。U+2233 */
}

NAOS_TEST(first_and_last_entries)
{
    NAOS_REQUIRE_UINT_GT(mnd_md_entity_count, 0);
    NAOS_CHECK_STR_EQ("AElig", mnd_md_entity_entries[0].name);
    NAOS_CHECK_STR_EQ("zwnj", mnd_md_entity_entries[mnd_md_entity_count - 1].name);
    check_found("AElig", "\xC3\x86");          /* U+00C6 */
    check_found("zwnj", "\xE2\x80\x8C");       /* U+200C */
}

NAOS_TEST(name_is_a_slice_not_nul_terminated)
{
    const unsigned char *utf8 = NULL;
    size_t len = 0;
    const char *text = "&ampersand;";

    /* 文書の一部を指す名前（直後に NUL がない）を、長さで区切って検索できる */
    NAOS_REQUIRE_TRUE(mnd_md_entity_lookup(text + 1, 3, &utf8, &len));
    NAOS_CHECK_MEM_EQ(utf8, len, "&", 1);
    NAOS_CHECK_FALSE(mnd_md_entity_lookup(text + 1, 9, &utf8, &len));
}

NAOS_TEST(unknown_names_are_not_found)
{
    check_not_found("notanentity", strlen("notanentity"));
    check_not_found("foo", 3);
    check_not_found("aMp", 3);                 /* 大文字と小文字を区別する */
    check_not_found("am", 2);                  /* 名前の先頭の一部 */
    check_not_found("Counter", 7);
    check_not_found("ampx", 4);                /* 名前の後ろに続く文字 */
    check_not_found("zzzz", 4);                /* 表の最後の名前より後ろ */
    check_not_found("AAAA", 4);                /* 表の最初の名前より前 */
}

NAOS_TEST(names_with_delimiters_are_not_found)
{
    /*
     * 表は `;` で終わるキーだけから作り、名前は `&` と `;` を除いた部分で引く。
     * `;` を含めた名前や `&` を含めた名前（entities.json の `&amp` のような `;` のないキーの形）は見つからない
     */
    check_not_found("amp;", 4);
    check_not_found("&amp", 4);
    check_not_found("&amp;", 5);
    check_not_found(";", 1);
}

NAOS_TEST(empty_and_too_long_names_are_not_found)
{
    char long_name[MND_MD_ENTITY_NAME_MAX + 2];

    check_not_found("", 0);
    check_not_found(NULL, 0);
    /* 最長の名前の後ろに 1 文字足した名前は、長さの上限を超える */
    memcpy(long_name, "CounterClockwiseContourIntegralx", MND_MD_ENTITY_NAME_MAX + 1);
    long_name[MND_MD_ENTITY_NAME_MAX + 1] = '\0';
    check_not_found(long_name, MND_MD_ENTITY_NAME_MAX + 1);
}

NAOS_TEST(table_has_only_semicolon_names)
{
    /*
     * entities.json の 2,231 個のキーのうち、`;` で終わるものは 2,125 個。`;` のない 106 個のキーは
     * どれも `;` 付きのキーと同じ名前なので、もし表に入れば名前が重複する（table_is_sorted_and_unique で検出する）
     */
    NAOS_CHECK_UINT_EQ(2125, mnd_md_entity_count);
}

NAOS_TEST(table_is_sorted_and_unique)
{
    size_t i = 0;

    NAOS_REQUIRE_UINT_GT(mnd_md_entity_count, 0);
    for (i = 0; i < mnd_md_entity_count; i++)
    {
        const mnd_md_entity_t *e = &mnd_md_entity_entries[i];
        size_t k = 0;
        bool alnum = true;

        NAOS_CHECK_TRUE((e->name_len >= 1) && (e->name_len <= MND_MD_ENTITY_NAME_MAX));
        NAOS_CHECK_UINT_EQ(e->name_len, strlen(e->name));
        NAOS_CHECK_TRUE((e->len >= 1) && (e->len <= MND_MD_ENTITY_UTF8_MAX));
        for (k = 0; k < e->name_len; k++)
        {
            char c = e->name[k];

            if (!(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9'))))
            {
                alnum = false;
            }
        }
        NAOS_CHECK_TRUE(alnum);
        if (i > 0)
        {
            NAOS_CHECK_INT_LT(strcmp(mnd_md_entity_entries[i - 1].name, e->name), 0);
        }
    }
}

NAOS_TEST(every_entry_is_found)
{
    size_t i = 0;

    for (i = 0; i < mnd_md_entity_count; i++)
    {
        const mnd_md_entity_t *e = &mnd_md_entity_entries[i];
        const unsigned char *utf8 = NULL;
        size_t len = 0;

        NAOS_CHECK_TRUE(mnd_md_entity_lookup(e->name, e->name_len, &utf8, &len));
        NAOS_CHECK_PTR_EQ(e->utf8, utf8);
        NAOS_CHECK_UINT_EQ(e->len, len);
    }
}

NAOS_SUITE(entity,
    NAOS_ADD_TEST(amp_is_ampersand),
    NAOS_ADD_TEST(ngE_is_two_codepoints),
    NAOS_ADD_TEST(multibyte_and_astral),
    NAOS_ADD_TEST(first_and_last_entries),
    NAOS_ADD_TEST(name_is_a_slice_not_nul_terminated),
    NAOS_ADD_TEST(unknown_names_are_not_found),
    NAOS_ADD_TEST(names_with_delimiters_are_not_found),
    NAOS_ADD_TEST(empty_and_too_long_names_are_not_found),
    NAOS_ADD_TEST(table_has_only_semicolon_names),
    NAOS_ADD_TEST(table_is_sorted_and_unique),
    NAOS_ADD_TEST(every_entry_is_found)
)
