/* Similarity: 問い合わせのマスク、bit-parallel LCS、上限、比較値 */

#include <naos/naos.h>

#include "po_merge/merge_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 擬似乱数による組の数と、各文字列のバイト長の上限 */
#define SIMILARITY_TEST_RANDOM_PAIRS 1000
#define SIMILARITY_TEST_RANDOM_MAX_LEN 300
/* 擬似乱数の固定の種 */
#define SIMILARITY_TEST_SEED UINT64_C(0x9E3779B97F4A7C15)
/* 構造のある長い文字列の領域の大きさ */
#define SIMILARITY_TEST_LONG_SIZE 512

/* xorshift64（固定の種で毎回同じ列を作る） */
static uint64_t next_random(uint64_t *state)
{
    uint64_t x = *state;

    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;

    return x;
}

/*
 * 文字の種類が alphabet のバイトを返す。256 種のときは 0 を除く全バイト値（0x80 以上を含む）、
 * それ以外は 'A' から始まる alphabet 種
 */
static char random_byte(uint64_t *state, unsigned alphabet)
{
    if (alphabet >= 256)
    {
        return (char)(unsigned char)(1u + (unsigned)(next_random(state) % 255u));
    }

    return (char)(unsigned char)(0x41u + (unsigned)(next_random(state) % alphabet));
}

/* 動的計画法による LCS（参照実装）。メモリ不足なら SIZE_MAX */
static size_t lcs_dp(const char *a, size_t alen, const char *b, size_t blen)
{
    size_t *prev;
    size_t *cur;
    size_t i;
    size_t j;
    size_t result;

    prev = calloc(blen + 1, sizeof(*prev));
    cur = calloc(blen + 1, sizeof(*cur));
    if ((prev == NULL) || (cur == NULL))
    {
        free(prev);
        free(cur);

        return SIZE_MAX;
    }

    for (i = 1; i <= alen; i++)
    {
        size_t *tmp;

        cur[0] = 0;
        for (j = 1; j <= blen; j++)
        {
            if (a[i - 1] == b[j - 1])
            {
                cur[j] = prev[j - 1] + 1;
            }
            else if (prev[j] >= cur[j - 1])
            {
                cur[j] = prev[j];
            }
            else
            {
                cur[j] = cur[j - 1];
            }
        }
        tmp = prev;
        prev = cur;
        cur = tmp;
    }

    result = prev[blen];
    free(prev);
    free(cur);

    return result;
}

/* pattern に q を設定し、c との LCS を返す。設定に失敗したら SIZE_MAX */
static size_t lcs_of(mnd_merge_pattern_t *pattern, const char *q, size_t qlen,
                     const char *c, size_t clen)
{
    if (!mnd_merge_pattern_set(pattern, q, qlen))
    {
        return SIZE_MAX;
    }

    return mnd_merge_lcs(pattern, c, clen);
}

/* 両方向の LCS が参照実装と等しく、上限の分子が 2 × LCS 以上であることを確かめる */
static void check_pair(mnd_merge_pattern_t *pattern, const char *a, size_t alen,
                       const char *b, size_t blen)
{
    size_t expected = lcs_dp(a, alen, b, blen);

    NAOS_REQUIRE_TRUE(expected != SIZE_MAX);
    NAOS_CHECK_UINT_EQ(expected, lcs_of(pattern, a, alen, b, blen));
    NAOS_CHECK_UINT_EQ(expected, lcs_of(pattern, b, blen, a, alen));
    NAOS_CHECK_UINT_GE(mnd_merge_upper_numerator(a, alen, b, blen), 2 * expected);
    NAOS_CHECK_UINT_GE(mnd_merge_upper_numerator(b, blen, a, alen), 2 * expected);
}

NAOS_TEST(similarity_scaffold_pattern_masks_are_64_bit_words)
{
    mnd_merge_pattern_t pattern;

    /* マスクの語は 64 ビットで、バイトの種類は 256 */
    NAOS_CHECK_UINT_EQ(sizeof(pattern.masks[0]), sizeof(uint64_t));
    NAOS_CHECK_UINT_EQ(MND_MERGE_PATTERN_WORD_BITS, 64);
    NAOS_CHECK_UINT_EQ(MND_MERGE_PATTERN_BYTE_KINDS, 256);
}

NAOS_TEST(similarity_empty_and_empty)
{
    mnd_merge_pattern_t pattern = { 0, 0, 0, NULL, NULL };

    NAOS_REQUIRE_TRUE(mnd_merge_pattern_set(&pattern, "", 0));
    NAOS_CHECK_UINT_EQ(0, pattern.len);
    NAOS_CHECK_UINT_EQ(0, pattern.words);
    NAOS_CHECK_UINT_EQ(0, mnd_merge_lcs(&pattern, "", 0));
    /* 両方が空なら 1.0 */
    NAOS_CHECK_TRUE(mnd_merge_ratio(0, 0) == 1.0);
    NAOS_CHECK_UINT_EQ(0, mnd_merge_upper_numerator("", 0, "", 0));
    mnd_merge_pattern_free(&pattern);
    NAOS_CHECK_NULL(pattern.masks);
    NAOS_CHECK_NULL(pattern.vector);
}

NAOS_TEST(similarity_one_side_empty)
{
    mnd_merge_pattern_t pattern = { 0, 0, 0, NULL, NULL };

    /* 問い合わせだけが空 */
    NAOS_CHECK_UINT_EQ(0, lcs_of(&pattern, "", 0, "abc", 3));
    /* 候補だけが空 */
    NAOS_CHECK_UINT_EQ(0, lcs_of(&pattern, "abc", 3, "", 0));
    /* 片方だけが空なら 0.0 */
    NAOS_CHECK_TRUE(mnd_merge_ratio(0, 3) == 0.0);
    NAOS_CHECK_UINT_EQ(0, mnd_merge_upper_numerator("abc", 3, "", 0));
    NAOS_CHECK_UINT_EQ(0, mnd_merge_upper_numerator("", 0, "abc", 3));
    mnd_merge_pattern_free(&pattern);
}

NAOS_TEST(similarity_identical)
{
    mnd_merge_pattern_t pattern = { 0, 0, 0, NULL, NULL };
    const char *s = "Hello, world";
    size_t len = strlen(s);

    NAOS_CHECK_UINT_EQ(len, lcs_of(&pattern, s, len, s, len));
    NAOS_CHECK_TRUE(mnd_merge_ratio(len, 2 * len) == 1.0);
    NAOS_CHECK_UINT_EQ(2 * len, mnd_merge_upper_numerator(s, len, s, len));
    mnd_merge_pattern_free(&pattern);
}

NAOS_TEST(similarity_no_common_bytes)
{
    mnd_merge_pattern_t pattern = { 0, 0, 0, NULL, NULL };

    NAOS_CHECK_UINT_EQ(0, lcs_of(&pattern, "abc", 3, "xyz", 3));
    NAOS_CHECK_TRUE(mnd_merge_ratio(0, 6) == 0.0);
    mnd_merge_pattern_free(&pattern);
}

NAOS_TEST(similarity_known_values)
{
    mnd_merge_pattern_t pattern = { 0, 0, 0, NULL, NULL };

    NAOS_CHECK_UINT_EQ(4, lcs_of(&pattern, "ABCBDAB", 7, "BDCABA", 6));
    NAOS_CHECK_UINT_EQ(4, lcs_of(&pattern, "kitten", 6, "sitting", 7));
    NAOS_CHECK_UINT_EQ(1, lcs_of(&pattern, "a", 1, "bab", 3));
    mnd_merge_pattern_free(&pattern);
}

NAOS_TEST(similarity_utf8_counts_bytes)
{
    mnd_merge_pattern_t pattern = { 0, 0, 0, NULL, NULL };
    /* 「日本語」は 9 バイト、「日本」は 6 バイト */
    const char *a = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    const char *b = "\xE6\x97\xA5\xE6\x9C\xAC";
    /* 「あ」（E3 81 82）と「い」（E3 81 84）は先頭の 2 バイトが共通 */
    const char *hira_a = "\xE3\x81\x82";
    const char *hira_i = "\xE3\x81\x84";

    NAOS_CHECK_UINT_EQ(6, lcs_of(&pattern, a, 9, b, 6));
    NAOS_CHECK_UINT_EQ(6, lcs_of(&pattern, b, 6, a, 9));
    NAOS_CHECK_UINT_EQ(2, lcs_of(&pattern, hira_a, 3, hira_i, 3));
    /* 0x80 以上のバイトのマスク（符号付きの char でも正しい位置） */
    NAOS_CHECK_UINT_EQ(3, lcs_of(&pattern, "\xFF\x80\xC0", 3, "\xFF\xFF\x80\x41\xC0", 5));
    NAOS_CHECK_UINT_EQ(2, lcs_of(&pattern, "\xFF\x80\xC0", 3, "\x80\xC0\xFF\xFF", 4));
    mnd_merge_pattern_free(&pattern);
}

NAOS_TEST(similarity_word_boundaries)
{
    mnd_merge_pattern_t pattern = { 0, 0, 0, NULL, NULL };
    char q[SIMILARITY_TEST_LONG_SIZE];
    char c[SIMILARITY_TEST_LONG_SIZE];
    static const size_t lengths[] = { 63, 64, 65, 127, 128, 129, 130, 200, 257 };
    size_t k;
    size_t i;

    for (k = 0; k < (sizeof(lengths) / sizeof(lengths[0])); k++)
    {
        size_t len = lengths[k];

        /* 同じ文字の列: LCS は長さそのもの */
        memset(q, 'a', len);
        NAOS_REQUIRE_TRUE(mnd_merge_pattern_set(&pattern, q, len));
        NAOS_CHECK_UINT_EQ(len, pattern.len);
        NAOS_CHECK_UINT_EQ((len + 63) / 64, pattern.words);
        NAOS_CHECK_UINT_GE(pattern.word_cap, pattern.words);
        NAOS_CHECK_UINT_EQ(len, mnd_merge_lcs(&pattern, q, len));
        /* 候補が問い合わせより長くても LCS は問い合わせの長さを超えない */
        memset(c, 'a', len + 10);
        NAOS_CHECK_UINT_EQ(len, mnd_merge_lcs(&pattern, c, len + 10));

        /* 問い合わせの末尾だけが一致する（桁上がりが全語を伝わる） */
        memset(q, 'x', len);
        q[len - 1] = 'a';
        check_pair(&pattern, q, len, "a", 1);
        check_pair(&pattern, q, len, "ya", 2);

        /* 繰り返しの模様（語の境界をまたぐ一致） */
        for (i = 0; i < len; i++)
        {
            q[i] = (char)('a' + (char)(i % 3));
        }
        for (i = 0; i < (len + 7); i++)
        {
            c[i] = (char)('a' + (char)((i * 7) % 4));
        }
        check_pair(&pattern, q, len, c, len + 7);
        check_pair(&pattern, q, len, c, len / 2);
    }
    mnd_merge_pattern_free(&pattern);
}

NAOS_TEST(similarity_pattern_reuses_capacity)
{
    mnd_merge_pattern_t pattern = { 0, 0, 0, NULL, NULL };
    char q[SIMILARITY_TEST_LONG_SIZE];
    size_t cap;

    memset(q, 'b', sizeof(q));
    NAOS_REQUIRE_TRUE(mnd_merge_pattern_set(&pattern, q, 300));
    NAOS_CHECK_UINT_EQ(5, pattern.words);
    cap = pattern.word_cap;
    NAOS_CHECK_UINT_GE(cap, 5);

    /* 短い問い合わせでは領域を再利用し、前の内容が残らない */
    NAOS_REQUIRE_TRUE(mnd_merge_pattern_set(&pattern, "ab", 2));
    NAOS_CHECK_UINT_EQ(2, pattern.len);
    NAOS_CHECK_UINT_EQ(1, pattern.words);
    NAOS_CHECK_UINT_EQ(cap, pattern.word_cap);
    NAOS_CHECK_UINT_EQ(1, mnd_merge_lcs(&pattern, "bbbb", 4));
    NAOS_CHECK_UINT_EQ(2, mnd_merge_lcs(&pattern, "xaxb", 4));
    /* マスクの配置は masks[b * words + w] */
    NAOS_CHECK_TRUE(pattern.masks[((size_t)'a' * pattern.words) + 0] == UINT64_C(1));
    NAOS_CHECK_TRUE(pattern.masks[((size_t)'b' * pattern.words) + 0] == UINT64_C(2));
    NAOS_CHECK_TRUE(pattern.masks[((size_t)'c' * pattern.words) + 0] == UINT64_C(0));

    /* 空にしても領域は残り、再び長くできる */
    NAOS_REQUIRE_TRUE(mnd_merge_pattern_set(&pattern, "", 0));
    NAOS_CHECK_UINT_EQ(0, mnd_merge_lcs(&pattern, "ab", 2));
    NAOS_REQUIRE_TRUE(mnd_merge_pattern_set(&pattern, q, 400));
    NAOS_CHECK_UINT_EQ(7, pattern.words);
    NAOS_CHECK_UINT_GE(pattern.word_cap, 7);
    NAOS_CHECK_UINT_EQ(400, mnd_merge_lcs(&pattern, q, 450));

    mnd_merge_pattern_free(&pattern);
    NAOS_CHECK_UINT_EQ(0, pattern.word_cap);
    mnd_merge_pattern_free(&pattern);
}

NAOS_TEST(similarity_random_pairs_match_dp)
{
    mnd_merge_pattern_t pattern = { 0, 0, 0, NULL, NULL };
    char a[SIMILARITY_TEST_RANDOM_MAX_LEN];
    char b[SIMILARITY_TEST_RANDOM_MAX_LEN];
    uint64_t state = SIMILARITY_TEST_SEED;
    size_t n;
    size_t i;

    for (n = 0; n < SIMILARITY_TEST_RANDOM_PAIRS; n++)
    {
        /* 文字の種類は 1〜4、16、256 のいずれか（小さいほど LCS が長くなる） */
        static const unsigned alphabets[] = { 1, 2, 3, 4, 16, 256 };
        unsigned alphabet = alphabets[next_random(&state) % 6];
        size_t alen = (size_t)(next_random(&state) % (SIMILARITY_TEST_RANDOM_MAX_LEN + 1));
        size_t blen = (size_t)(next_random(&state) % (SIMILARITY_TEST_RANDOM_MAX_LEN + 1));

        for (i = 0; i < alen; i++)
        {
            a[i] = random_byte(&state, alphabet);
        }
        for (i = 0; i < blen; i++)
        {
            b[i] = random_byte(&state, alphabet);
        }
        check_pair(&pattern, a, alen, b, blen);
    }
    mnd_merge_pattern_free(&pattern);
}

NAOS_TEST(similarity_upper_numerator_length_bound)
{
    /* 長さの和が 20 未満では長さの上限 2 × min だけ */
    NAOS_CHECK_UINT_EQ(6, mnd_merge_upper_numerator("abc", 3, "xyzxyz", 6));
    NAOS_CHECK_UINT_EQ(18, mnd_merge_upper_numerator("aaaaaaaaaa", 10, "bbbbbbbbb", 9));
}

NAOS_TEST(similarity_upper_numerator_count_bound)
{
    /* 長さの和が 20 以上では出現数の上限 |a| + |b| − D も使う */
    NAOS_CHECK_UINT_EQ(0, mnd_merge_upper_numerator("aaaaaaaaaa", 10, "bbbbbbbbbb", 10));
    /* a: a×10 b×5、b: a×5 b×10 → D = 10、分子 = 30 − 10 = 20（長さの上限 30 より小さい） */
    NAOS_CHECK_UINT_EQ(20, mnd_merge_upper_numerator("aaaaaaaaaabbbbb", 15,
                                                     "aaaaabbbbbbbbbb", 15));
    /* 出現数が同じなら長さの上限と等しい */
    NAOS_CHECK_UINT_EQ(24, mnd_merge_upper_numerator("abcdefghijkl", 12, "lkjihgfedcba", 12));
    /* 長さの上限の方が小さい場合 */
    NAOS_CHECK_UINT_EQ(10, mnd_merge_upper_numerator("aaaaa", 5, "aaaaaaaaaaaaaaaaaaaa", 20));
}

NAOS_TEST(similarity_ratio_values)
{
    /* (double)(2 × numerator) / (double)sum と同じ値 */
    NAOS_CHECK_TRUE(mnd_merge_ratio(3, 10) == ((double)6 / (double)10));
    NAOS_CHECK_TRUE(mnd_merge_ratio(1, 3) == ((double)2 / (double)3));
    NAOS_CHECK_TRUE(mnd_merge_ratio(7, 20) == ((double)14 / (double)20));
    /* 上限の分子（偶数）は numerator = 分子 / 2 で同じ形の値になる */
    NAOS_CHECK_TRUE(mnd_merge_ratio(20 / 2, 30) == ((double)20 / (double)30));
}

NAOS_SUITE(similarity,
    NAOS_ADD_TEST(similarity_scaffold_pattern_masks_are_64_bit_words),
    NAOS_ADD_TEST(similarity_empty_and_empty),
    NAOS_ADD_TEST(similarity_one_side_empty),
    NAOS_ADD_TEST(similarity_identical),
    NAOS_ADD_TEST(similarity_no_common_bytes),
    NAOS_ADD_TEST(similarity_known_values),
    NAOS_ADD_TEST(similarity_utf8_counts_bytes),
    NAOS_ADD_TEST(similarity_word_boundaries),
    NAOS_ADD_TEST(similarity_pattern_reuses_capacity),
    NAOS_ADD_TEST(similarity_random_pairs_match_dp),
    NAOS_ADD_TEST(similarity_upper_numerator_length_bound),
    NAOS_ADD_TEST(similarity_upper_numerator_count_bound),
    NAOS_ADD_TEST(similarity_ratio_values)
)
