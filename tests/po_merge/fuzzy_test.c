/* Fuzzy: 候補の登録、4-gram の索引、候補を調べる順序と最良の候補の選択 */

#include <naos/naos.h>

#include "po_merge/merge_internal.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 4-gram の列を探す。なければ NULL */
static const mnd_merge_index_list_t *find_gram_list(const mnd_merge_fuzzy_t *fuzzy,
                                                    const char *gram)
{
    size_t id = 0;

    if (!mnd_merge_hash_find(&fuzzy->grams, gram, strlen(gram), &id))
    {
        return NULL;
    }

    NAOS_REQUIRE_TRUE(id < fuzzy->gram_list_count);

    return &fuzzy->gram_lists[id];
}

/* 候補の番号 candidate が list にあるか */
static bool list_has(const mnd_merge_index_list_t *list, size_t candidate)
{
    size_t i;

    for (i = 0; i < list->count; i++)
    {
        if (list->items[i] == candidate)
        {
            return true;
        }
    }

    return false;
}

/* 候補 candidate を含むバイト長ごとの列の数 */
static size_t count_length_lists_with(const mnd_merge_fuzzy_t *fuzzy, size_t candidate)
{
    size_t n = 0;
    size_t i;

    for (i = 0; i <= MND_MERGE_FUZZY_SHORT_MAX_LEN; i++)
    {
        if (list_has(&fuzzy->by_length[i], candidate))
        {
            n++;
        }
    }

    return n;
}

NAOS_TEST(fuzzy_scaffold_has_length_list_per_short_byte_length)
{
    mnd_merge_fuzzy_t fuzzy;

    /* 4-gram は 4 文字、バイト長 0〜28 の候補ごとに列を持つ */
    NAOS_CHECK_UINT_EQ(MND_MERGE_FUZZY_GRAM_CHARS, 4);
    NAOS_CHECK_UINT_EQ(MND_MERGE_FUZZY_SHORT_MAX_LEN, 28);
    NAOS_CHECK_UINT_EQ(sizeof(fuzzy.by_length) / sizeof(fuzzy.by_length[0]), 29);
}

NAOS_TEST(fuzzy_init_registers_translated_entries_only)
{
    static const char po[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"untranslated entry\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"translated entry\"\n"
        "msgstr \"訳\"\n"
        "\n"
        "#, fuzzy\n"
        "msgid \"fuzzy entry\"\n"
        "msgstr \"訳\"\n"
        "\n"
        "msgid \"plural one\"\n"
        "msgid_plural \"plural many\"\n"
        "msgstr[0] \"\"\n"
        "msgstr[1] \"訳\"\n"
        "\n"
        "msgid \"plural two\"\n"
        "msgid_plural \"plural twos\"\n"
        "msgstr[0] \"訳\"\n"
        "msgstr[1] \"\"\n"
        "\n"
        "#~ msgid \"obsolete entry\"\n"
        "#~ msgstr \"訳\"\n"
        "\n"
        "#~ msgid \"obsolete untranslated\"\n"
        "#~ msgstr \"\"\n";
    mnd_po_catalog_t catalog;
    mnd_merge_fuzzy_t fuzzy;
    size_t i;

    TEST_READ_PO(po, &catalog);
    NAOS_REQUIRE_UINT_EQ(8, catalog.count);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &catalog));
    NAOS_CHECK_PTR_EQ(&catalog, fuzzy.catalog);

    /*
     * ヘッダ（0）、未翻訳（1）、msgstr[0] が空の複数形（4）、未翻訳の廃止エントリ（7）は
     * 登録しない。fuzzy のエントリと廃止エントリは訳があるので登録する（添字の昇順）
     */
    NAOS_REQUIRE_UINT_EQ(4, fuzzy.candidate_count);
    NAOS_CHECK_UINT_EQ(2, fuzzy.candidates[0]);
    NAOS_CHECK_UINT_EQ(3, fuzzy.candidates[1]);
    NAOS_CHECK_UINT_EQ(5, fuzzy.candidates[2]);
    NAOS_CHECK_UINT_EQ(6, fuzzy.candidates[3]);

    /* 作業領域は候補の数だけあり、数え上げは 0 */
    NAOS_REQUIRE_NOT_NULL(fuzzy.counts);
    NAOS_REQUIRE_NOT_NULL(fuzzy.touched);
    NAOS_CHECK_UINT_EQ(0, fuzzy.touched_count);
    for (i = 0; i < fuzzy.candidate_count; i++)
    {
        NAOS_CHECK_UINT_EQ(0, fuzzy.counts[i]);
    }

    /* 登録しないエントリの 4-gram は索引にない */
    NAOS_CHECK_NULL(find_gram_list(&fuzzy, "untr"));
    NAOS_CHECK_NULL(find_gram_list(&fuzzy, "l on"));
    NAOS_CHECK_NULL(find_gram_list(&fuzzy, "e un"));
    /* 登録したエントリの 4-gram は索引にある（番号は candidates の添字） */
    NAOS_REQUIRE_NOT_NULL(find_gram_list(&fuzzy, "obso"));
    NAOS_CHECK_UINT_EQ(1, find_gram_list(&fuzzy, "obso")->count);
    NAOS_CHECK_UINT_EQ(3, find_gram_list(&fuzzy, "obso")->items[0]);
    NAOS_REQUIRE_NOT_NULL(find_gram_list(&fuzzy, "fuzz"));
    NAOS_CHECK_UINT_EQ(1, find_gram_list(&fuzzy, "fuzz")->items[0]);
    /* 複数の候補が持つ 4-gram の列は昇順 */
    NAOS_REQUIRE_NOT_NULL(find_gram_list(&fuzzy, "ntry"));
    NAOS_REQUIRE_UINT_EQ(3, find_gram_list(&fuzzy, "ntry")->count);
    NAOS_CHECK_UINT_EQ(0, find_gram_list(&fuzzy, "ntry")->items[0]);
    NAOS_CHECK_UINT_EQ(1, find_gram_list(&fuzzy, "ntry")->items[1]);
    NAOS_CHECK_UINT_EQ(3, find_gram_list(&fuzzy, "ntry")->items[2]);

    /* ヘッダ（バイト長 0）は長さの列にも入らない */
    NAOS_CHECK_UINT_EQ(0, fuzzy.by_length[0].count);
    /* バイト長の列: "translated entry" は 16、"fuzzy entry" は 11 */
    NAOS_REQUIRE_UINT_EQ(1, fuzzy.by_length[16].count);
    NAOS_CHECK_UINT_EQ(0, fuzzy.by_length[16].items[0]);
    NAOS_REQUIRE_UINT_EQ(1, fuzzy.by_length[11].count);
    NAOS_CHECK_UINT_EQ(1, fuzzy.by_length[11].items[0]);
    /* "untranslated entry"（18）と "obsolete untranslated"（21）は登録しない */
    NAOS_CHECK_UINT_EQ(0, fuzzy.by_length[18].count);
    NAOS_CHECK_UINT_EQ(0, fuzzy.by_length[21].count);

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_merge_fuzzy_free(&fuzzy);
    NAOS_CHECK_NULL(fuzzy.candidates);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(fuzzy_init_lists_repeated_gram_once)
{
    static const char po[] =
        "msgid \"abcdabcd\"\n"
        "msgstr \"x\"\n"
        "\n"
        "msgid \"zabcd\"\n"
        "msgstr \"y\"\n";
    mnd_po_catalog_t catalog;
    mnd_merge_fuzzy_t fuzzy;
    const mnd_merge_index_list_t *list;

    TEST_READ_PO(po, &catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &catalog));
    NAOS_REQUIRE_UINT_EQ(2, fuzzy.candidate_count);

    /* "abcdabcd" は abcd を 2 回含むが、列には 1 回だけ入る */
    list = find_gram_list(&fuzzy, "abcd");
    NAOS_REQUIRE_NOT_NULL(list);
    NAOS_REQUIRE_UINT_EQ(2, list->count);
    NAOS_CHECK_UINT_EQ(0, list->items[0]);
    NAOS_CHECK_UINT_EQ(1, list->items[1]);

    /* 4-gram の種類: abcd、bcda、cdab、dabc、zabc（各位置から 1 つ、重複は 1 つの列） */
    NAOS_CHECK_UINT_EQ(5, fuzzy.grams.count);
    NAOS_CHECK_UINT_EQ(5, fuzzy.gram_list_count);
    NAOS_REQUIRE_NOT_NULL(find_gram_list(&fuzzy, "bcda"));
    NAOS_REQUIRE_NOT_NULL(find_gram_list(&fuzzy, "cdab"));
    NAOS_REQUIRE_NOT_NULL(find_gram_list(&fuzzy, "dabc"));
    NAOS_REQUIRE_NOT_NULL(find_gram_list(&fuzzy, "zabc"));
    NAOS_CHECK_UINT_EQ(1, find_gram_list(&fuzzy, "dabc")->count);
    NAOS_CHECK_UINT_EQ(1, find_gram_list(&fuzzy, "zabc")->items[0]);
    /* 3 文字や 5 文字のバイト列は 4-gram ではない */
    NAOS_CHECK_NULL(find_gram_list(&fuzzy, "abc"));
    NAOS_CHECK_NULL(find_gram_list(&fuzzy, "abcda"));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(fuzzy_init_grams_are_utf8_characters)
{
    static const char po[] =
        "msgid \"日本語\"\n"
        "msgstr \"Japanese\"\n"
        "\n"
        "msgid \"日本語です\"\n"
        "msgstr \"It is Japanese\"\n"
        "\n"
        "msgid \"aé日b\"\n"
        "msgstr \"mixed\"\n";
    mnd_po_catalog_t catalog;
    mnd_merge_fuzzy_t fuzzy;
    const mnd_merge_index_list_t *list;

    TEST_READ_PO(po, &catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &catalog));
    NAOS_REQUIRE_UINT_EQ(3, fuzzy.candidate_count);

    /* 3 文字の日本語（9 バイト）は 4-gram の索引に入らず、長さ 9 の列に入る */
    NAOS_REQUIRE_UINT_EQ(1, fuzzy.by_length[9].count);
    NAOS_CHECK_UINT_EQ(0, fuzzy.by_length[9].items[0]);
    NAOS_CHECK_NULL(find_gram_list(&fuzzy, "日本語"));

    /* 5 文字（15 バイト）は 4 文字（12 バイト）の 4-gram を 2 つ持つ */
    list = find_gram_list(&fuzzy, "日本語で");
    NAOS_REQUIRE_NOT_NULL(list);
    NAOS_REQUIRE_UINT_EQ(1, list->count);
    NAOS_CHECK_UINT_EQ(1, list->items[0]);
    list = find_gram_list(&fuzzy, "本語です");
    NAOS_REQUIRE_NOT_NULL(list);
    NAOS_CHECK_UINT_EQ(1, list->items[0]);
    NAOS_REQUIRE_UINT_EQ(1, fuzzy.by_length[15].count);
    NAOS_CHECK_UINT_EQ(1, fuzzy.by_length[15].items[0]);

    /* 1〜3 バイトの文字が混ざった 4 文字（7 バイト）は 4-gram を 1 つだけ持つ */
    list = find_gram_list(&fuzzy, "aé日b");
    NAOS_REQUIRE_NOT_NULL(list);
    NAOS_CHECK_UINT_EQ(2, list->items[0]);
    NAOS_REQUIRE_UINT_EQ(1, fuzzy.by_length[7].count);
    NAOS_CHECK_UINT_EQ(2, fuzzy.by_length[7].items[0]);

    /* 4-gram は 3 つだけ（文字の途中で切れたバイト列は入らない） */
    NAOS_CHECK_UINT_EQ(3, fuzzy.grams.count);
    NAOS_CHECK_UINT_EQ(3, fuzzy.gram_list_count);

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(fuzzy_init_length_list_boundary)
{
    static const char po[] =
        "msgid \"aaaaaaaaaaaaaaaaaaaaaaaaaaab\"\n"
        "msgstr \"28\"\n"
        "\n"
        "msgid \"aaaaaaaaaaaaaaaaaaaaaaaaaaabc\"\n"
        "msgstr \"29\"\n"
        "\n"
        "msgid \"cccccccccccccccccccccccccccc\"\n"
        "msgstr \"28\"\n";
    mnd_po_catalog_t catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(po, &catalog);
    NAOS_REQUIRE_UINT_EQ(28, catalog.entries[0].msgid.len);
    NAOS_REQUIRE_UINT_EQ(29, catalog.entries[1].msgid.len);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &catalog));
    NAOS_REQUIRE_UINT_EQ(3, fuzzy.candidate_count);

    /* バイト長 28 の候補は列に入り（添字の昇順）、29 の候補はどの列にも入らない */
    NAOS_REQUIRE_UINT_EQ(2, fuzzy.by_length[28].count);
    NAOS_CHECK_UINT_EQ(0, fuzzy.by_length[28].items[0]);
    NAOS_CHECK_UINT_EQ(2, fuzzy.by_length[28].items[1]);
    NAOS_CHECK_UINT_EQ(1, count_length_lists_with(&fuzzy, 0));
    NAOS_CHECK_UINT_EQ(0, count_length_lists_with(&fuzzy, 1));

    /* 29 バイトの候補も 4-gram の索引には入る */
    NAOS_REQUIRE_NOT_NULL(find_gram_list(&fuzzy, "aabc"));
    NAOS_CHECK_UINT_EQ(1, find_gram_list(&fuzzy, "aabc")->items[0]);
    /* "aaaa" は候補 0 と 1 の列（同じ候補の繰り返しは 1 回） */
    NAOS_REQUIRE_NOT_NULL(find_gram_list(&fuzzy, "aaaa"));
    NAOS_REQUIRE_UINT_EQ(2, find_gram_list(&fuzzy, "aaaa")->count);
    NAOS_CHECK_UINT_EQ(0, find_gram_list(&fuzzy, "aaaa")->items[0]);
    NAOS_CHECK_UINT_EQ(1, find_gram_list(&fuzzy, "aaaa")->items[1]);
    NAOS_REQUIRE_NOT_NULL(find_gram_list(&fuzzy, "cccc"));
    NAOS_CHECK_UINT_EQ(1, find_gram_list(&fuzzy, "cccc")->count);

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&catalog);
}

NAOS_TEST(fuzzy_init_empty_catalog)
{
    mnd_po_catalog_t catalog;
    mnd_merge_fuzzy_t fuzzy;
    size_t i;

    mnd_po_catalog_init(&catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &catalog));
    NAOS_CHECK_UINT_EQ(0, fuzzy.candidate_count);
    NAOS_CHECK_UINT_EQ(0, fuzzy.gram_list_count);
    NAOS_CHECK_UINT_EQ(0, fuzzy.grams.count);
    for (i = 0; i <= MND_MERGE_FUZZY_SHORT_MAX_LEN; i++)
    {
        NAOS_CHECK_UINT_EQ(0, fuzzy.by_length[i].count);
    }
    mnd_merge_fuzzy_free(&fuzzy);
    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&catalog);
}

NAOS_SUITE(fuzzy,
    NAOS_ADD_TEST(fuzzy_scaffold_has_length_list_per_short_byte_length),
    NAOS_ADD_TEST(fuzzy_init_registers_translated_entries_only),
    NAOS_ADD_TEST(fuzzy_init_lists_repeated_gram_once),
    NAOS_ADD_TEST(fuzzy_init_grams_are_utf8_characters),
    NAOS_ADD_TEST(fuzzy_init_length_list_boundary),
    NAOS_ADD_TEST(fuzzy_init_empty_catalog)
)
