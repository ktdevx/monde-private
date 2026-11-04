/* Hash と KeyIndex: バイト列キーのハッシュ表、msgctxt と msgid による索引 */

#include <naos/naos.h>

#include "po_merge/merge_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* 容量の拡大を何度も起こすキーの数 */
#define INDEX_TEST_MANY_KEYS 1000
/* キーの文字列の領域の大きさ */
#define INDEX_TEST_KEY_SIZE 16

NAOS_TEST(index_scaffold_key_separator_is_eot)
{
    /* キーは「msgctxt の有無の 1 バイト + msgctxt + 0x04 + msgid」の連結 */
    NAOS_CHECK_UINT_EQ((unsigned char)MND_MERGE_KEY_SEPARATOR, 0x04u);
}

NAOS_TEST(index_hash_init_is_empty)
{
    mnd_merge_hash_t hash;
    size_t value = 99;

    mnd_merge_hash_init(&hash);
    NAOS_CHECK_NULL(hash.slots);
    NAOS_CHECK_UINT_EQ(0, hash.cap);
    NAOS_CHECK_UINT_EQ(0, hash.count);
    /* 空の表の検索は見つからない */
    NAOS_CHECK_FALSE(mnd_merge_hash_find(&hash, "a", 1, &value));
    NAOS_CHECK_UINT_EQ(99, value);
    mnd_merge_hash_free(&hash);
    mnd_merge_hash_free(&hash);
    NAOS_CHECK_NULL(hash.slots);
}

NAOS_TEST(index_hash_insert_and_find)
{
    mnd_merge_hash_t hash;
    bool inserted = false;
    size_t existing = 0;
    size_t value = 0;

    mnd_merge_hash_init(&hash);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, "apple", 5, 10, &inserted, &existing));
    NAOS_CHECK_TRUE(inserted);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, "banana", 6, 20, &inserted, &existing));
    NAOS_CHECK_TRUE(inserted);
    NAOS_CHECK_UINT_EQ(2, hash.count);

    NAOS_CHECK_TRUE(mnd_merge_hash_find(&hash, "apple", 5, &value));
    NAOS_CHECK_UINT_EQ(10, value);
    NAOS_CHECK_TRUE(mnd_merge_hash_find(&hash, "banana", 6, &value));
    NAOS_CHECK_UINT_EQ(20, value);
    /* 容量は 2 の累乗で、使用率は 1/2 以下 */
    NAOS_CHECK_TRUE((hash.cap & (hash.cap - 1)) == 0);
    NAOS_CHECK_UINT_LE(hash.count * 2, hash.cap);
    mnd_merge_hash_free(&hash);
}

NAOS_TEST(index_hash_find_missing_key)
{
    mnd_merge_hash_t hash;
    bool inserted = false;
    size_t existing = 0;
    size_t value = 77;

    mnd_merge_hash_init(&hash);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, "apple", 5, 1, &inserted, &existing));
    NAOS_CHECK_FALSE(mnd_merge_hash_find(&hash, "apples", 6, &value));
    /* 先頭の部分だけ一致するキー、長さだけ違うキーは別のキー */
    NAOS_CHECK_FALSE(mnd_merge_hash_find(&hash, "appl", 4, &value));
    NAOS_CHECK_FALSE(mnd_merge_hash_find(&hash, "apple", 0, &value));
    NAOS_CHECK_FALSE(mnd_merge_hash_find(&hash, "Apple", 5, &value));
    NAOS_CHECK_UINT_EQ(77, value);
    mnd_merge_hash_free(&hash);
}

NAOS_TEST(index_hash_existing_key_keeps_earlier_value)
{
    mnd_merge_hash_t hash;
    bool inserted = true;
    size_t existing = 0;
    size_t value = 0;
    /* 同じ内容で別の領域のキー（内容で比べる） */
    char other[] = "key";

    mnd_merge_hash_init(&hash);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, "key", 3, 5, &inserted, &existing));
    NAOS_CHECK_TRUE(inserted);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, other, 3, 8, &inserted, &existing));
    NAOS_CHECK_FALSE(inserted);
    NAOS_CHECK_UINT_EQ(5, existing);
    NAOS_CHECK_UINT_EQ(1, hash.count);
    NAOS_CHECK_TRUE(mnd_merge_hash_find(&hash, "key", 3, &value));
    NAOS_CHECK_UINT_EQ(5, value);
    mnd_merge_hash_free(&hash);
}

NAOS_TEST(index_hash_keys_are_not_copied)
{
    mnd_merge_hash_t hash;
    bool inserted = false;
    size_t existing = 0;
    const char *key = "pointer";

    mnd_merge_hash_init(&hash);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, key, 7, 3, &inserted, &existing));
    NAOS_REQUIRE_UINT_EQ(1, hash.count);
    {
        size_t i;
        const unsigned char *stored = NULL;

        for (i = 0; i < hash.cap; i++)
        {
            if (hash.slots[i].used)
            {
                stored = hash.slots[i].key;
            }
        }

        NAOS_CHECK_PTR_EQ((const void *)key, (const void *)stored);
    }

    mnd_merge_hash_free(&hash);
}

NAOS_TEST(index_hash_values_survive_growth)
{
    static char keys[INDEX_TEST_MANY_KEYS][INDEX_TEST_KEY_SIZE];
    mnd_merge_hash_t hash;
    bool inserted = false;
    size_t existing = 0;
    size_t value = 0;
    size_t first_cap = 0;
    size_t i;
    size_t failures = 0;

    mnd_merge_hash_init(&hash);
    for (i = 0; i < INDEX_TEST_MANY_KEYS; i++)
    {
        (void)snprintf(keys[i], sizeof(keys[i]), "key-%zu", i);
        NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, keys[i], strlen(keys[i]), i * 3,
                                                &inserted, &existing));
        if (!inserted)
        {
            failures++;
        }

        if (i == 0)
        {
            first_cap = hash.cap;
        }

        if ((hash.count * 2) > hash.cap)
        {
            failures++;
        }
    }

    NAOS_CHECK_UINT_EQ(0, failures);
    NAOS_CHECK_UINT_EQ(INDEX_TEST_MANY_KEYS, hash.count);
    /* 容量の拡大が何度も起きている（初めの容量の 16 倍以上） */
    NAOS_CHECK_UINT_GE(hash.cap, first_cap * 16);
    NAOS_CHECK_TRUE((hash.cap & (hash.cap - 1)) == 0);

    for (i = 0; i < INDEX_TEST_MANY_KEYS; i++)
    {
        value = (size_t)-1;
        if (!mnd_merge_hash_find(&hash, keys[i], strlen(keys[i]), &value) || (value != (i * 3)))
        {
            failures++;
        }
    }

    NAOS_CHECK_UINT_EQ(0, failures);

    /* 拡大の後も既存のキーの挿入は先の値を返す */
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, keys[7], strlen(keys[7]), 12345,
                                            &inserted, &existing));
    NAOS_CHECK_FALSE(inserted);
    NAOS_CHECK_UINT_EQ(21, existing);
    NAOS_CHECK_UINT_EQ(INDEX_TEST_MANY_KEYS, hash.count);
    mnd_merge_hash_free(&hash);
    NAOS_CHECK_NULL(hash.slots);
    NAOS_CHECK_UINT_EQ(0, hash.cap);
    NAOS_CHECK_UINT_EQ(0, hash.count);
}

NAOS_TEST(index_hash_zero_length_key)
{
    mnd_merge_hash_t hash;
    bool inserted = false;
    size_t existing = 0;
    size_t value = 0;

    mnd_merge_hash_init(&hash);
    NAOS_CHECK_FALSE(mnd_merge_hash_find(&hash, "", 0, &value));
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, "", 0, 4, &inserted, &existing));
    NAOS_CHECK_TRUE(inserted);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, "x", 1, 9, &inserted, &existing));
    NAOS_CHECK_TRUE(inserted);
    NAOS_CHECK_TRUE(mnd_merge_hash_find(&hash, "", 0, &value));
    NAOS_CHECK_UINT_EQ(4, value);
    /* 長さ 0 のキーはポインタによらず同じキー（NULL も可） */
    NAOS_CHECK_TRUE(mnd_merge_hash_find(&hash, NULL, 0, &value));
    NAOS_CHECK_UINT_EQ(4, value);
    NAOS_CHECK_TRUE(mnd_merge_hash_find(&hash, "zzz", 0, &value));
    NAOS_CHECK_UINT_EQ(4, value);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, NULL, 0, 6, &inserted, &existing));
    NAOS_CHECK_FALSE(inserted);
    NAOS_CHECK_UINT_EQ(4, existing);
    NAOS_CHECK_UINT_EQ(2, hash.count);
    mnd_merge_hash_free(&hash);
}

NAOS_TEST(index_hash_null_key_first)
{
    mnd_merge_hash_t hash;
    bool inserted = false;
    size_t existing = 0;
    size_t value = 0;

    mnd_merge_hash_init(&hash);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, NULL, 0, 2, &inserted, &existing));
    NAOS_CHECK_TRUE(inserted);
    NAOS_CHECK_TRUE(mnd_merge_hash_find(&hash, "", 0, &value));
    NAOS_CHECK_UINT_EQ(2, value);
    mnd_merge_hash_free(&hash);
}

NAOS_TEST(index_hash_binary_keys)
{
    mnd_merge_hash_t hash;
    bool inserted = false;
    size_t existing = 0;
    size_t value = 0;
    /* NUL と 0x04 を含むバイト列も内容と長さで区別する */
    static const unsigned char a[] = {0x00, 0x04, 'a'};
    static const unsigned char b[] = {0x01, 0x04, 'a'};
    static const unsigned char c[] = {0x00, 0x04, 'a', 0x00};

    mnd_merge_hash_init(&hash);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, a, sizeof(a), 1, &inserted, &existing));
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, b, sizeof(b), 2, &inserted, &existing));
    NAOS_CHECK_TRUE(inserted);
    NAOS_REQUIRE_TRUE(mnd_merge_hash_insert(&hash, c, sizeof(c), 3, &inserted, &existing));
    NAOS_CHECK_TRUE(inserted);
    NAOS_CHECK_TRUE(mnd_merge_hash_find(&hash, a, sizeof(a), &value));
    NAOS_CHECK_UINT_EQ(1, value);
    NAOS_CHECK_TRUE(mnd_merge_hash_find(&hash, b, sizeof(b), &value));
    NAOS_CHECK_UINT_EQ(2, value);
    NAOS_CHECK_TRUE(mnd_merge_hash_find(&hash, c, sizeof(c), &value));
    NAOS_CHECK_UINT_EQ(3, value);
    mnd_merge_hash_free(&hash);
}

NAOS_SUITE(index,
    NAOS_ADD_TEST(index_scaffold_key_separator_is_eot),
    NAOS_ADD_TEST(index_hash_init_is_empty),
    NAOS_ADD_TEST(index_hash_insert_and_find),
    NAOS_ADD_TEST(index_hash_find_missing_key),
    NAOS_ADD_TEST(index_hash_existing_key_keeps_earlier_value),
    NAOS_ADD_TEST(index_hash_keys_are_not_copied),
    NAOS_ADD_TEST(index_hash_values_survive_growth),
    NAOS_ADD_TEST(index_hash_zero_length_key),
    NAOS_ADD_TEST(index_hash_null_key_first),
    NAOS_ADD_TEST(index_hash_binary_keys)
)
