/* Unicode: 一般カテゴリの判定と大文字小文字の畳み込み */

#include <naos/naos.h>

#include "markdown/md_unicode.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* cp を畳み込んだ列が expected（NUL 終端の UTF-8）と一致するかを確かめる */
static void check_fold(uint32_t cp, const char *expected)
{
    char out[MND_MD_UNICODE_FOLD_MAX];
    size_t len = 0;

    memset(out, 0, sizeof(out));
    len = mnd_md_unicode_fold(cp, out);
    NAOS_CHECK_MEM_EQ(out, len, expected, strlen(expected));
}

NAOS_TEST(punct_ascii_p_and_s)
{
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x0021));  /* ! (Po) */
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x0024));  /* $ (Sc) */
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x002A));  /* * (Po) */
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x005F));  /* _ (Pc) */
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x002B));  /* + (Sm) */
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x005E));  /* ^ (Sk) */
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x007E));  /* ~ (Sm) */
}

NAOS_TEST(punct_non_ascii)
{
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x3002));   /* 。(Po) */
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x00A9));   /* © (So) */
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x20AC));   /* € (Sc) */
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x300C));   /* 「(Ps) */
    NAOS_CHECK_TRUE(mnd_md_unicode_is_punct(0x1F600));  /* 😀 (So) */
}

NAOS_TEST(punct_excludes_letters_digits_and_spaces)
{
    NAOS_CHECK_FALSE(mnd_md_unicode_is_punct(0x0041));  /* A (Lu) */
    NAOS_CHECK_FALSE(mnd_md_unicode_is_punct(0x0030));  /* 0 (Nd) */
    NAOS_CHECK_FALSE(mnd_md_unicode_is_punct(0x0020));  /* 空白 (Zs) */
    NAOS_CHECK_FALSE(mnd_md_unicode_is_punct(0x3042));  /* あ (Lo) */
    NAOS_CHECK_FALSE(mnd_md_unicode_is_punct(0x0301));  /* 結合文字 (Mn) */
    NAOS_CHECK_FALSE(mnd_md_unicode_is_punct(0x0378));  /* 未割り当て (Cn) */
    NAOS_CHECK_FALSE(mnd_md_unicode_is_punct(0x110000));
}

NAOS_TEST(space_is_zs)
{
    NAOS_CHECK_TRUE(mnd_md_unicode_is_space(0x0020));
    NAOS_CHECK_TRUE(mnd_md_unicode_is_space(0x00A0));
    NAOS_CHECK_TRUE(mnd_md_unicode_is_space(0x2000));
    NAOS_CHECK_TRUE(mnd_md_unicode_is_space(0x200A));
    NAOS_CHECK_TRUE(mnd_md_unicode_is_space(0x3000));
}

NAOS_TEST(space_excludes_other_categories)
{
    NAOS_CHECK_FALSE(mnd_md_unicode_is_space(0x0009));  /* タブ (Cc) */
    NAOS_CHECK_FALSE(mnd_md_unicode_is_space(0x000A));  /* LF (Cc) */
    NAOS_CHECK_FALSE(mnd_md_unicode_is_space(0x200B));  /* ZERO WIDTH SPACE (Cf) */
    NAOS_CHECK_FALSE(mnd_md_unicode_is_space(0x2028));  /* LINE SEPARATOR (Zl) */
    NAOS_CHECK_FALSE(mnd_md_unicode_is_space(0x0041));
    NAOS_CHECK_FALSE(mnd_md_unicode_is_space(0x110000));
}

NAOS_TEST(fold_simple_c)
{
    check_fold(0x0041, "a");
    check_fold(0x005A, "z");
    check_fold(0x00C0, "\xC3\xA0");          /* À → à */
    check_fold(0x0391, "\xCE\xB1");          /* Α → α */
    check_fold(0x00B5, "\xCE\xBC");          /* µ → μ */
    check_fold(0xFF21, "\xEF\xBD\x81");      /* Ａ → ａ */
    check_fold(0x10400, "\xF0\x90\x90\xA8"); /* 𐐀 → 𐐨 */
}

NAOS_TEST(fold_full_f_is_multi_codepoint)
{
    check_fold(0x1E9E, "ss");              /* ẞ → ss（S の ß ではなく F を使う） */
    check_fold(0x00DF, "ss");              /* ß → ss */
    check_fold(0x0130, "i\xCC\x87");       /* İ → i + U+0307（T は使わない） */
    check_fold(0xFB03, "ffi");             /* ﬃ → ffi */
    check_fold(0x0390, "\xCE\xB9\xCC\x88\xCC\x81"); /* ΐ → 3 つのコードポイント（6 バイト） */
}

NAOS_TEST(fold_unmapped_is_identity)
{
    check_fold(0x0061, "a");
    check_fold(0x0021, "!");
    check_fold(0x3042, "\xE3\x81\x82");       /* あ */
    check_fold(0x1F600, "\xF0\x9F\x98\x80");  /* 😀 */
    check_fold(0x10FFFF, "\xF4\x8F\xBF\xBF");
}

NAOS_TEST(fold_nul_writes_one_byte)
{
    char out[MND_MD_UNICODE_FOLD_MAX];

    memset(out, 'x', sizeof(out));
    NAOS_CHECK_UINT_EQ(1, mnd_md_unicode_fold(0x0000, out));
    NAOS_CHECK_INT_EQ(0, out[0]);
}

NAOS_TEST(fold_invalid_codepoint_writes_nothing)
{
    char out[MND_MD_UNICODE_FOLD_MAX];

    memset(out, 'x', sizeof(out));
    NAOS_CHECK_UINT_EQ(0, mnd_md_unicode_fold(0x110000, out));
    NAOS_CHECK_UINT_EQ(0, mnd_md_unicode_fold(0xD800, out));
    NAOS_CHECK_UINT_EQ(0, mnd_md_unicode_fold(0xDFFF, out));
    NAOS_CHECK_UINT_EQ(0, mnd_md_unicode_fold(UINT32_MAX, out));
    NAOS_CHECK_INT_EQ('x', out[0]);
}

NAOS_TEST(tables_are_sorted_and_disjoint)
{
    size_t i = 0;

    NAOS_REQUIRE_UINT_GT(mnd_md_unicode_punct_count, 0);
    NAOS_REQUIRE_UINT_GT(mnd_md_unicode_space_count, 0);
    NAOS_REQUIRE_UINT_GT(mnd_md_unicode_fold_count, 0);
    for (i = 0; i < mnd_md_unicode_punct_count; i++)
    {
        NAOS_CHECK_TRUE(mnd_md_unicode_punct_ranges[i].first <= mnd_md_unicode_punct_ranges[i].last);
        if (i > 0)
        {
            /* 隣り合う範囲はまとめてあるので、間に 1 つ以上の隙間がある */
            NAOS_CHECK_TRUE((mnd_md_unicode_punct_ranges[i - 1].last + 1) < mnd_md_unicode_punct_ranges[i].first);
        }
    }
    for (i = 0; i < mnd_md_unicode_space_count; i++)
    {
        NAOS_CHECK_TRUE(mnd_md_unicode_space_ranges[i].first <= mnd_md_unicode_space_ranges[i].last);
        if (i > 0)
        {
            NAOS_CHECK_TRUE((mnd_md_unicode_space_ranges[i - 1].last + 1) < mnd_md_unicode_space_ranges[i].first);
        }
    }
    for (i = 0; i < mnd_md_unicode_fold_count; i++)
    {
        NAOS_CHECK_TRUE((mnd_md_unicode_fold_entries[i].len >= 1)
                        && (mnd_md_unicode_fold_entries[i].len <= MND_MD_UNICODE_FOLD_MAX));
        if (i > 0)
        {
            NAOS_CHECK_TRUE(mnd_md_unicode_fold_entries[i - 1].cp < mnd_md_unicode_fold_entries[i].cp);
        }
    }
}

NAOS_SUITE(unicode,
    NAOS_ADD_TEST(punct_ascii_p_and_s),
    NAOS_ADD_TEST(punct_non_ascii),
    NAOS_ADD_TEST(punct_excludes_letters_digits_and_spaces),
    NAOS_ADD_TEST(space_is_zs),
    NAOS_ADD_TEST(space_excludes_other_categories),
    NAOS_ADD_TEST(fold_simple_c),
    NAOS_ADD_TEST(fold_full_f_is_multi_codepoint),
    NAOS_ADD_TEST(fold_unmapped_is_identity),
    NAOS_ADD_TEST(fold_nul_writes_one_byte),
    NAOS_ADD_TEST(fold_invalid_codepoint_writes_nothing),
    NAOS_ADD_TEST(tables_are_sorted_and_disjoint)
)
