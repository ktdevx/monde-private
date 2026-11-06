/* Similarity: 問い合わせのマスク、bit-parallel LCS、上限、比較値 */

#include "po_merge/merge_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 出現数による上限を使う長さの和の下限 */
#define MND_MERGE_COUNT_BOUND_MIN_SUM 20u

/* x の 1 のビットの数（コンパイラの組み込み関数を使わない） */
static size_t popcount64(uint64_t x)
{
    x = x - ((x >> 1) & UINT64_C(0x5555555555555555));
    x = (x & UINT64_C(0x3333333333333333)) + ((x >> 2) & UINT64_C(0x3333333333333333));
    x = (x + (x >> 4)) & UINT64_C(0x0F0F0F0F0F0F0F0F);

    return (size_t)((x * UINT64_C(0x0101010101010101)) >> 56);
}

/* len ビットの最後の語の有効なビットのマスク（len は 1 以上） */
static uint64_t last_word_mask(size_t len)
{
    size_t rest = len % MND_MERGE_PATTERN_WORD_BITS;

    if (rest == 0)
    {
        return ~UINT64_C(0);
    }

    return (UINT64_C(1) << rest) - 1;
}

/*
 * 語数 words の領域を用意する。足りなければ新しく確保し、失敗時は pattern を変えずに false。
 * 確保の前に 256 × words 語の大きさのオーバーフローを検査する
 */
static bool reserve_words(mnd_merge_pattern_t *pattern, size_t words)
{
    uint64_t *masks;
    uint64_t *vector;

    if (words <= pattern->word_cap)
    {
        return true;
    }

    if (words > ((SIZE_MAX / sizeof(uint64_t)) / MND_MERGE_PATTERN_BYTE_KINDS))
    {
        return false;
    }

    masks = malloc(words * MND_MERGE_PATTERN_BYTE_KINDS * sizeof(uint64_t));
    vector = malloc(words * sizeof(uint64_t));
    if ((masks == NULL) || (vector == NULL))
    {
        free(masks);
        free(vector);

        return false;
    }

    free(pattern->masks);
    free(pattern->vector);
    pattern->masks = masks;
    pattern->vector = vector;
    pattern->word_cap = words;

    return true;
}

bool mnd_merge_pattern_set(mnd_merge_pattern_t *pattern, const char *s, size_t len)
{
    const unsigned char *p = (const unsigned char *)s;
    size_t words = (len / MND_MERGE_PATTERN_WORD_BITS)
                   + (((len % MND_MERGE_PATTERN_WORD_BITS) != 0) ? 1u : 0u);
    size_t i;

    if (!reserve_words(pattern, words))
    {
        return false;
    }

    pattern->len = len;
    pattern->words = words;
    if (words == 0)
    {
        return true;
    }

    /* バイト b の語 w は masks[(b * words) + w]。位置 i はビット i（語 i / 64 のビット i % 64） */
    memset(pattern->masks, 0, words * MND_MERGE_PATTERN_BYTE_KINDS * sizeof(uint64_t));
    for (i = 0; i < len; i++)
    {
        size_t word = i / MND_MERGE_PATTERN_WORD_BITS;
        size_t bit = i % MND_MERGE_PATTERN_WORD_BITS;

        pattern->masks[((size_t)p[i] * words) + word] |= UINT64_C(1) << bit;
    }

    return true;
}

void mnd_merge_pattern_free(mnd_merge_pattern_t *pattern)
{
    free(pattern->masks);
    free(pattern->vector);
    pattern->len = 0;
    pattern->words = 0;
    pattern->word_cap = 0;
    pattern->masks = NULL;
    pattern->vector = NULL;
}

/*
 * bit-parallel LCS。V（|q| ビット、初期値はすべて 1）を候補の各バイト c について
 * U = V & M[c]、V = (V + U) | (V − U) で更新し、LCS = |q| − popcount(V) とする。
 * 語の列の加算は桁上がりを、減算は桁借りを下位の語から上位の語へ伝える
 */
size_t mnd_merge_lcs(const mnd_merge_pattern_t *pattern, const char *s, size_t len)
{
    const unsigned char *p = (const unsigned char *)s;
    size_t words = pattern->words;
    uint64_t *v = pattern->vector;
    uint64_t top_mask;
    size_t ones;
    size_t i;
    size_t w;

    if ((words == 0) || (len == 0))
    {
        return 0;
    }

    top_mask = last_word_mask(pattern->len);
    for (w = 0; w < words; w++)
    {
        v[w] = ~UINT64_C(0);
    }

    v[words - 1] = top_mask;

    for (i = 0; i < len; i++)
    {
        const uint64_t *m = &pattern->masks[(size_t)p[i] * words];
        uint64_t carry = 0;
        uint64_t borrow = 0;

        for (w = 0; w < words; w++)
        {
            uint64_t vw = v[w];
            uint64_t u = vw & m[w];
            uint64_t sum = vw + u;
            uint64_t sum_carry = (sum < vw) ? 1u : 0u;
            uint64_t diff = vw - u;
            uint64_t diff_borrow = (vw < u) ? 1u : 0u;

            sum += carry;
            sum_carry |= (sum < carry) ? 1u : 0u;
            diff_borrow |= (diff < borrow) ? 1u : 0u;
            diff -= borrow;
            v[w] = sum | diff;
            carry = sum_carry;
            borrow = diff_borrow;
        }

        /* 問い合わせの長さを超えるビットへの桁上がりを捨てる */
        v[words - 1] &= top_mask;
    }

    ones = 0;
    for (w = 0; w < words; w++)
    {
        ones += popcount64(v[w]);
    }

    return pattern->len - ones;
}

/*
 * (double)(2 × numerator) / (double)sum と同じ値を返す。2 倍は double の上で行う
 * （2 の累乗の倍は丸めと可換なので、整数で 2 倍してから変換した値と等しく、size_t の
 * オーバーフローもない）
 */
double mnd_merge_ratio(size_t numerator, size_t sum)
{
    if (sum == 0)
    {
        return 1.0;
    }

    return (2.0 * (double)numerator) / (double)sum;
}

/*
 * 2 × LCS(a, b) の上限。長さの上限 2 × min(|a|, |b|) と、長さの和が 20 以上のときは
 * 出現数の上限 |a| + |b| − D（D は 256 種のバイトの出現数の差の絶対値の和）の小さい方。
 * |a| + |b| − D は各バイトの 2 × min(出現数) の和に等しいので、その形で計算する
 * （どちらの上限も偶数。長さの和が size_t に収まる前提で、オーバーフローしない）
 */
size_t mnd_merge_upper_numerator(const char *a, size_t alen, const char *b, size_t blen)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    size_t count_a[MND_MERGE_PATTERN_BYTE_KINDS];
    size_t count_b[MND_MERGE_PATTERN_BYTE_KINDS];
    size_t min_len = (alen < blen) ? alen : blen;
    size_t upper = 2 * min_len;
    size_t common;
    size_t i;

    /* alen + blen >= 20（加算のオーバーフローを避けて判定する） */
    if ((alen < MND_MERGE_COUNT_BOUND_MIN_SUM)
        && (blen < (MND_MERGE_COUNT_BOUND_MIN_SUM - alen)))
    {
        return upper;
    }

    memset(count_a, 0, sizeof(count_a));
    memset(count_b, 0, sizeof(count_b));

    for (i = 0; i < alen; i++)
    {
        count_a[pa[i]]++;
    }

    for (i = 0; i < blen; i++)
    {
        count_b[pb[i]]++;
    }

    common = 0;
    for (i = 0; i < MND_MERGE_PATTERN_BYTE_KINDS; i++)
    {
        common += (count_a[i] < count_b[i]) ? count_a[i] : count_b[i];
    }

    /* common <= min_len なので 2 × common もオーバーフローしない */
    if ((2 * common) < upper)
    {
        upper = 2 * common;
    }

    return upper;
}
