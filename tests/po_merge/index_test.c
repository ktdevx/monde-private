/* Hash と KeyIndex: バイト列キーのハッシュ表、msgctxt と msgid による索引 */

#include <naos/naos.h>

#include "po_merge/merge_internal.h"
#include "test_util.h"

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

/* KeyIndex */

/*
 * catalog の末尾にエントリを加える。msgctxt が NULL なら msgctxt なし。
 * 文字列は長さで渡す（0x04 などを含められるように）
 */
static void index_test_add_entry(mnd_po_catalog_t *catalog, const char *msgctxt,
                                 size_t msgctxt_len, const char *msgid, size_t msgid_len,
                                 bool obsolete)
{
    mnd_po_entry_t *entry = mnd_po_catalog_append(catalog);

    NAOS_REQUIRE_NOT_NULL(entry);
    if (msgctxt != NULL)
    {
        entry->has_msgctxt = true;
        NAOS_REQUIRE_TRUE(mnd_buf_append(&entry->msgctxt, msgctxt, msgctxt_len));
    }

    NAOS_REQUIRE_TRUE(mnd_buf_append(&entry->msgid, msgid, msgid_len));
    NAOS_REQUIRE_TRUE(mnd_po_strings_push(&entry->msgstr, "", 0));
    entry->obsolete = obsolete;
}

/* NULL なら 0、そうでなければ strlen */
static size_t index_test_len(const char *s)
{
    if (s == NULL)
    {
        return 0;
    }

    return strlen(s);
}

/* 文字列リテラル（NUL を含まない）を長さとともに渡す形 */
#define INDEX_TEST_ADD(catalog, msgctxt, msgid, obsolete) \
    index_test_add_entry((catalog), (msgctxt), index_test_len(msgctxt), \
                         (msgid), strlen(msgid), (obsolete))

/* catalog のすべてのエントリを添字の順に登録し、すべて新しく登録されたことを確かめる */
static void index_test_add_all(mnd_merge_key_index_t *index, const mnd_po_catalog_t *catalog)
{
    size_t i;
    bool added = false;
    size_t existing = 0;

    for (i = 0; i < catalog->count; i++)
    {
        NAOS_REQUIRE_TRUE(mnd_merge_key_index_add(index, i, &added, &existing));
        NAOS_CHECK_TRUE(added);
    }
}

/* query（msgctxt が NULL なら msgctxt なし）で検索した結果 */
static bool index_test_find(const mnd_merge_key_index_t *index, const char *msgctxt,
                            size_t msgctxt_len, const char *msgid, size_t msgid_len,
                            size_t *entry_index)
{
    mnd_po_entry_t query;
    bool found = false;
    bool ok = true;

    mnd_po_entry_init(&query);
    if (msgctxt != NULL)
    {
        query.has_msgctxt = true;
        ok = mnd_buf_append(&query.msgctxt, msgctxt, msgctxt_len);
    }

    ok = ok && mnd_buf_append(&query.msgid, msgid, msgid_len);
    NAOS_CHECK_TRUE(ok);
    if (ok)
    {
        found = mnd_merge_key_index_find(index, &query, entry_index);
    }

    mnd_po_entry_free(&query);

    return found;
}

#define INDEX_TEST_FIND(index, msgctxt, msgid, entry_index) \
    index_test_find((index), (msgctxt), index_test_len(msgctxt), (msgid), \
                    strlen(msgid), (entry_index))

NAOS_TEST(index_key_empty_catalog)
{
    mnd_po_catalog_t catalog;
    mnd_merge_key_index_t index;
    size_t found = 42;

    mnd_po_catalog_init(&catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_init(&index, &catalog));
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, NULL, "", &found));
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, "", "", &found));
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, NULL, "a", &found));
    NAOS_CHECK_UINT_EQ(42, found);
    mnd_merge_key_index_free(&index);
    mnd_merge_key_index_free(&index);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(index_key_absent_and_empty_msgctxt_are_distinct)
{
    mnd_po_catalog_t catalog;
    mnd_merge_key_index_t index;
    size_t found = 0;

    /* msgctxt のないキーと空の msgctxt のキーは別のキー */
    TEST_READ_PO("msgid \"file\"\n"
                 "msgstr \"a\"\n"
                 "\n"
                 "msgctxt \"\"\n"
                 "msgid \"file\"\n"
                 "msgstr \"b\"\n"
                 "\n"
                 "msgctxt \"menu\"\n"
                 "msgid \"file\"\n"
                 "msgstr \"c\"\n",
                 &catalog);
    NAOS_REQUIRE_UINT_EQ(3, catalog.count);
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_init(&index, &catalog));
    index_test_add_all(&index, &catalog);

    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "file", &found));
    NAOS_CHECK_UINT_EQ(0, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, "", "file", &found));
    NAOS_CHECK_UINT_EQ(1, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, "menu", "file", &found));
    NAOS_CHECK_UINT_EQ(2, found);
    /* エントリそのものを問い合わせにしても同じ添字 */
    NAOS_CHECK_TRUE(mnd_merge_key_index_find(&index, &catalog.entries[1], &found));
    NAOS_CHECK_UINT_EQ(1, found);
    mnd_merge_key_index_free(&index);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(index_key_only_empty_msgctxt_registered)
{
    mnd_po_catalog_t catalog;
    mnd_merge_key_index_t index;
    size_t found = 7;

    /* 空の msgctxt のキーだけを登録すると、msgctxt のないキーは見つからない（逆も同じ） */
    mnd_po_catalog_init(&catalog);
    INDEX_TEST_ADD(&catalog, "", "x", false);
    INDEX_TEST_ADD(&catalog, NULL, "y", false);
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_init(&index, &catalog));
    index_test_add_all(&index, &catalog);
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, NULL, "x", &found));
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, "", "y", &found));
    NAOS_CHECK_UINT_EQ(7, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, "", "x", &found));
    NAOS_CHECK_UINT_EQ(0, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "y", &found));
    NAOS_CHECK_UINT_EQ(1, found);
    mnd_merge_key_index_free(&index);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(index_key_second_registration_returns_first_index)
{
    mnd_po_catalog_t catalog;
    mnd_merge_key_index_t index;
    bool added = false;
    size_t existing = 99;
    size_t found = 0;

    mnd_po_catalog_init(&catalog);
    INDEX_TEST_ADD(&catalog, "ctx", "same", false);
    INDEX_TEST_ADD(&catalog, NULL, "other", false);
    INDEX_TEST_ADD(&catalog, "ctx", "same", false);
    INDEX_TEST_ADD(&catalog, "ctx", "same", false);
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_init(&index, &catalog));

    NAOS_REQUIRE_TRUE(mnd_merge_key_index_add(&index, 0, &added, &existing));
    NAOS_CHECK_TRUE(added);
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_add(&index, 1, &added, &existing));
    NAOS_CHECK_TRUE(added);
    NAOS_CHECK_UINT_EQ(99, existing);

    /* 2 回目の登録は上書きせず、先の添字を返す（3 回目も同じ） */
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_add(&index, 2, &added, &existing));
    NAOS_CHECK_FALSE(added);
    NAOS_CHECK_UINT_EQ(0, existing);
    added = true;
    existing = 99;
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_add(&index, 3, &added, &existing));
    NAOS_CHECK_FALSE(added);
    NAOS_CHECK_UINT_EQ(0, existing);

    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, "ctx", "same", &found));
    NAOS_CHECK_UINT_EQ(0, found);
    NAOS_CHECK_UINT_EQ(2, index.hash.count);
    mnd_merge_key_index_free(&index);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(index_key_registration_order_expresses_precedence)
{
    mnd_po_catalog_t catalog;
    mnd_merge_key_index_t index;
    bool added = false;
    size_t existing = 0;
    size_t found = 0;
    size_t i;

    /*
     * 手順 2 の登録の順: 廃止エントリでないエントリを先に、次に廃止エントリを添字の順に。
     * 添字では廃止エントリが先にあっても、通常のエントリが優先し、廃止エントリどうしは先のものが勝つ
     */
    mnd_po_catalog_init(&catalog);
    INDEX_TEST_ADD(&catalog, NULL, "revived", true);   /* 0 */
    INDEX_TEST_ADD(&catalog, NULL, "normal", true);    /* 1 */
    INDEX_TEST_ADD(&catalog, NULL, "normal", false);   /* 2 */
    INDEX_TEST_ADD(&catalog, NULL, "revived", true);   /* 3 */
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_init(&index, &catalog));
    for (i = 0; i < catalog.count; i++)
    {
        if (!catalog.entries[i].obsolete)
        {
            NAOS_REQUIRE_TRUE(mnd_merge_key_index_add(&index, i, &added, &existing));
            NAOS_CHECK_TRUE(added);
        }
    }

    for (i = 0; i < catalog.count; i++)
    {
        if (catalog.entries[i].obsolete)
        {
            NAOS_REQUIRE_TRUE(mnd_merge_key_index_add(&index, i, &added, &existing));
        }
    }

    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "normal", &found));
    NAOS_CHECK_UINT_EQ(2, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "revived", &found));
    NAOS_CHECK_UINT_EQ(0, found);
    mnd_merge_key_index_free(&index);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(index_key_missing_key)
{
    mnd_po_catalog_t catalog;
    mnd_merge_key_index_t index;
    size_t found = 55;
    bool added = false;
    size_t existing = 0;

    mnd_po_catalog_init(&catalog);
    INDEX_TEST_ADD(&catalog, "menu", "open", false);
    INDEX_TEST_ADD(&catalog, NULL, "close", false);
    INDEX_TEST_ADD(&catalog, NULL, "unregistered", false);
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_init(&index, &catalog));

    /* 何も登録していなければ、カタログにあるキーも見つからない */
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, NULL, "close", &found));
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_add(&index, 0, &added, &existing));
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_add(&index, 1, &added, &existing));

    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, NULL, "open", &found));
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, "", "open", &found));
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, "Menu", "open", &found));
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, "menu", "ope", &found));
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, "menu", "opened", &found));
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, "close", "", &found));
    /* カタログにあるが登録していないキー */
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, NULL, "unregistered", &found));
    /* カタログのどのキーより長いキー（作業領域の外。領域を確保せずに「ない」） */
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, "a much longer context than any key", "open", &found));
    NAOS_CHECK_UINT_EQ(55, found);

    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, "menu", "open", &found));
    NAOS_CHECK_UINT_EQ(0, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "close", &found));
    NAOS_CHECK_UINT_EQ(1, found);
    mnd_merge_key_index_free(&index);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(index_key_find_with_longest_key)
{
    mnd_po_catalog_t catalog;
    mnd_merge_key_index_t index;
    size_t found = 0;

    /* 最も長いキーの検索は作業領域をちょうど使い切る */
    mnd_po_catalog_init(&catalog);
    INDEX_TEST_ADD(&catalog, NULL, "s", false);
    INDEX_TEST_ADD(&catalog, "context", "the longest message", false);
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_init(&index, &catalog));
    index_test_add_all(&index, &catalog);
    NAOS_CHECK_UINT_EQ(1 + 7 + 1 + 19, index.max_key_len);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, "context", "the longest message", &found));
    NAOS_CHECK_UINT_EQ(1, found);
    /* 1 バイト長いキー */
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, "context", "the longest message!", &found));
    /* msgctxt の有無だけ違う同じ長さのキー（"context" の代わりに msgctxt なしで 1 バイト短い） */
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, NULL, "the longest message", &found));
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "s", &found));
    NAOS_CHECK_UINT_EQ(0, found);
    mnd_merge_key_index_free(&index);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(index_key_separator_boundaries)
{
    mnd_po_catalog_t catalog;
    mnd_merge_key_index_t index;
    size_t found = 0;

    /*
     * 有無の 1 バイトにより、msgctxt "a" + msgid "b" と、msgctxt なしの msgid "a\x04b" は別のキー。
     * 区切りの位置により、msgctxt "ab" + msgid "" と msgctxt "a" + msgid "b" も別のキー
     */
    mnd_po_catalog_init(&catalog);
    INDEX_TEST_ADD(&catalog, "a", "b", false);           /* 0 */
    INDEX_TEST_ADD(&catalog, NULL, "a\x04" "b", false);   /* 1 */
    INDEX_TEST_ADD(&catalog, "ab", "", false);           /* 2 */
    INDEX_TEST_ADD(&catalog, NULL, "\x01" "a", false);   /* 3 */
    INDEX_TEST_ADD(&catalog, "", "", false);             /* 4 */
    INDEX_TEST_ADD(&catalog, NULL, "", false);           /* 5 */
    INDEX_TEST_ADD(&catalog, NULL, "\x04", false);       /* 6 */
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_init(&index, &catalog));
    index_test_add_all(&index, &catalog);
    NAOS_CHECK_UINT_EQ(7, index.hash.count);

    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, "a", "b", &found));
    NAOS_CHECK_UINT_EQ(0, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "a\x04" "b", &found));
    NAOS_CHECK_UINT_EQ(1, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, "ab", "", &found));
    NAOS_CHECK_UINT_EQ(2, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "\x01" "a", &found));
    NAOS_CHECK_UINT_EQ(3, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, "", "", &found));
    NAOS_CHECK_UINT_EQ(4, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "", &found));
    NAOS_CHECK_UINT_EQ(5, found);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "\x04", &found));
    NAOS_CHECK_UINT_EQ(6, found);
    mnd_merge_key_index_free(&index);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(index_key_header_is_an_ordinary_key)
{
    mnd_po_catalog_t catalog;
    mnd_merge_key_index_t index;
    size_t found = 0;

    /* 索引はヘッダを特別に扱わない（ヘッダを登録しないのは呼び出し側の役目） */
    TEST_READ_PO("msgid \"\"\n"
                 "msgstr \"\"\n"
                 "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                 "\n"
                 "msgid \"x\"\n"
                 "msgstr \"y\"\n",
                 &catalog);
    NAOS_REQUIRE_UINT_EQ(2, catalog.count);
    NAOS_REQUIRE_TRUE(mnd_merge_key_index_init(&index, &catalog));
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, NULL, "", &found));
    index_test_add_all(&index, &catalog);
    NAOS_CHECK_TRUE(INDEX_TEST_FIND(&index, NULL, "", &found));
    NAOS_CHECK_UINT_EQ(0, found);
    NAOS_CHECK_FALSE(INDEX_TEST_FIND(&index, "", "", &found));
    mnd_merge_key_index_free(&index);
    mnd_po_catalog_free(&catalog);
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
    NAOS_ADD_TEST(index_hash_binary_keys),
    NAOS_ADD_TEST(index_key_empty_catalog),
    NAOS_ADD_TEST(index_key_absent_and_empty_msgctxt_are_distinct),
    NAOS_ADD_TEST(index_key_only_empty_msgctxt_registered),
    NAOS_ADD_TEST(index_key_second_registration_returns_first_index),
    NAOS_ADD_TEST(index_key_registration_order_expresses_precedence),
    NAOS_ADD_TEST(index_key_missing_key),
    NAOS_ADD_TEST(index_key_find_with_longest_key),
    NAOS_ADD_TEST(index_key_separator_boundaries),
    NAOS_ADD_TEST(index_key_header_is_an_ordinary_key)
)
