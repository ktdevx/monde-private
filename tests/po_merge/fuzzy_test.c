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

/* 検索で見つからなかったことを表す値 */
#define FUZZY_TEST_NOT_FOUND ((size_t)-1)

/* 作業領域が検索の外の状態（数え上げがすべて 0、触れた候補が 0 個）であることを確かめる */
static void check_work_area_clean(const mnd_merge_fuzzy_t *fuzzy)
{
    size_t i;

    NAOS_CHECK_UINT_EQ(0, fuzzy->touched_count);
    for (i = 0; i < fuzzy->candidate_count; i++)
    {
        NAOS_CHECK_UINT_EQ(0, fuzzy->counts[i]);
    }
}

/* query で検索し、見つかった catalog の添字を返す。見つからなければ FUZZY_TEST_NOT_FOUND */
static size_t search(mnd_merge_fuzzy_t *fuzzy, const mnd_po_entry_t *query)
{
    bool has_found = true;
    size_t found = 0;

    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_search(fuzzy, query, &has_found, &found));
    check_work_area_clean(fuzzy);
    if (!has_found)
    {
        NAOS_CHECK_UINT_EQ(0, found);

        return FUZZY_TEST_NOT_FOUND;
    }

    return found;
}

/*
 * query について調べる候補の番号の順が expected（count 個）と等しいことを確かめる。
 * 確かめた後で作業領域を検索の外の状態に戻す
 */
static void check_order(mnd_merge_fuzzy_t *fuzzy, const mnd_po_entry_t *query,
                        const size_t *expected, size_t count)
{
    size_t i;

    mnd_merge_fuzzy_order(fuzzy, query);
    for (i = 0; i < fuzzy->candidate_count; i++)
    {
        NAOS_CHECK_UINT_EQ(0, fuzzy->counts[i]);
    }
    NAOS_REQUIRE_UINT_EQ(count, fuzzy->touched_count);
    for (i = 0; i < count; i++)
    {
        NAOS_CHECK_UINT_EQ(expected[i], fuzzy->touched[i]);
    }
    fuzzy->touched_count = 0;
}

/* a と b の類似度（2 × LCS / (|a| + |b|)） */
static double similarity(const char *a, const char *b)
{
    mnd_merge_pattern_t pattern;
    size_t lcs;

    memset(&pattern, 0, sizeof(pattern));
    NAOS_REQUIRE_TRUE(mnd_merge_pattern_set(&pattern, a, strlen(a)));
    lcs = mnd_merge_lcs(&pattern, b, strlen(b));
    mnd_merge_pattern_free(&pattern);

    return mnd_merge_ratio(lcs, strlen(a) + strlen(b));
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

NAOS_TEST(fuzzy_search_does_not_choose_similarity_exactly_threshold)
{
    static const char old_po[] =
        "msgctxt \"other\"\n"
        "msgid \"abcdefwxyz\"\n"
        "msgstr \"x\"\n";
    static const char template_po[] =
        "msgid \"abcdefghij\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgctxt \"other\"\n"
        "msgid \"abcdefghij\"\n"
        "msgstr \"\"\n";
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    /* LCS は 6、長さの和は 20 で、類似度は 0.6 ちょうど */
    NAOS_CHECK_TRUE(similarity("abcdefghij", "abcdefwxyz") == 0.6);
    /* msgctxt が異なるので bonus はなく、比較値 0.6 は最良値 0.6 を超えない */
    NAOS_CHECK_UINT_EQ(FUZZY_TEST_NOT_FOUND, search(&fuzzy, &template_catalog.entries[0]));
    /* 同じ msgctxt なら bonus を加えた 0.60001 が 0.6 を超える */
    NAOS_CHECK_UINT_EQ(0, search(&fuzzy, &template_catalog.entries[1]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_search_ignores_candidates_without_shared_gram)
{
    static const char old_po[] =
        "msgid \"abcZdefZgh\"\n"
        "msgstr \"x\"\n";
    static const char template_po[] =
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"abcZdefZgX\"\n"
        "msgstr \"\"\n";
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    /* 類似度は 16 / 18 だが、4-gram を 1 つも共有しないので調べない */
    NAOS_CHECK_TRUE(similarity("abcdefgh", "abcZdefZgh") > 0.6);
    check_order(&fuzzy, &template_catalog.entries[0], NULL, 0);
    NAOS_CHECK_UINT_EQ(FUZZY_TEST_NOT_FOUND, search(&fuzzy, &template_catalog.entries[0]));
    /* 4-gram を共有する問い合わせでは選ぶ */
    NAOS_CHECK_UINT_EQ(0, search(&fuzzy, &template_catalog.entries[1]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_order_sorts_by_count_then_candidate_number)
{
    static const char old_po[] =
        "msgid \"abcdQ\"\n"
        "msgstr \"0\"\n"
        "\n"
        "msgid \"Qdefghij\"\n"
        "msgstr \"1\"\n"
        "\n"
        "msgid \"abcdeQ\"\n"
        "msgstr \"2\"\n"
        "\n"
        "msgid \"Qghij\"\n"
        "msgstr \"3\"\n"
        "\n"
        "msgid \"QQQQ\"\n"
        "msgstr \"4\"\n"
        "\n"
        "msgid \"cdefghQ\"\n"
        "msgstr \"5\"\n"
        "\n"
        "msgid \"bcdeQ\"\n"
        "msgstr \"6\"\n"
        "\n"
        "msgid \"bcdaQ\"\n"
        "msgstr \"7\"\n";
    static const char template_po[] =
        "msgid \"abcdefghij\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"abcdabcd\"\n"
        "msgstr \"\"\n";
    /* 数え上げ: 1 は 4、5 は 3、2 は 2、0・3・6 は 1、4 と 7 は 0 */
    static const size_t expected_long[] = { 1, 5, 2, 0, 3, 6 };
    /* "abcdabcd" は abcd を 2 回含むので、0 は 2、2 も 2（abcd を 2 回）、7 は 1（bcda） */
    static const size_t expected_repeated[] = { 0, 2, 7 };
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));
    NAOS_REQUIRE_UINT_EQ(8, fuzzy.candidate_count);

    check_order(&fuzzy, &template_catalog.entries[0], expected_long,
                sizeof(expected_long) / sizeof(expected_long[0]));
    check_order(&fuzzy, &template_catalog.entries[1], expected_repeated,
                sizeof(expected_repeated) / sizeof(expected_repeated[0]));
    /* 2 回目も同じ順（作業領域が前の結果に影響しない） */
    check_order(&fuzzy, &template_catalog.entries[0], expected_long,
                sizeof(expected_long) / sizeof(expected_long[0]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_search_tie_prefers_higher_count)
{
    static const char old_po[] =
        "msgid \"abcdeXgh\"\n"
        "msgstr \"x\"\n"
        "\n"
        "msgid \"abcdefgX\"\n"
        "msgstr \"y\"\n";
    static const char template_po[] =
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n";
    static const size_t expected[] = { 1, 0 };
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    /* どちらも LCS 7 で類似度は同じ。後の候補は 4-gram を 4 つ、先の候補は 2 つ共有する */
    NAOS_CHECK_TRUE(similarity("abcdefgh", "abcdeXgh") == similarity("abcdefgh", "abcdefgX"));
    check_order(&fuzzy, &template_catalog.entries[0], expected, 2);
    NAOS_CHECK_UINT_EQ(1, search(&fuzzy, &template_catalog.entries[0]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_search_tie_with_same_count_prefers_earlier)
{
    static const char old_po[] =
        "msgid \"abcdefgX\"\n"
        "msgstr \"x\"\n"
        "\n"
        "msgid \"abcdefgY\"\n"
        "msgstr \"y\"\n";
    static const char reversed_po[] =
        "msgid \"abcdefgY\"\n"
        "msgstr \"y\"\n"
        "\n"
        "msgid \"abcdefgX\"\n"
        "msgstr \"x\"\n";
    static const char template_po[] =
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n";
    static const size_t expected[] = { 0, 1 };
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t reversed_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(reversed_po, &reversed_catalog);
    TEST_READ_PO(template_po, &template_catalog);

    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));
    check_order(&fuzzy, &template_catalog.entries[0], expected, 2);
    NAOS_CHECK_UINT_EQ(0, search(&fuzzy, &template_catalog.entries[0]));
    mnd_merge_fuzzy_free(&fuzzy);

    /* 並びを入れ替えても、先の候補が残る */
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &reversed_catalog));
    check_order(&fuzzy, &template_catalog.entries[0], expected, 2);
    NAOS_CHECK_UINT_EQ(0, search(&fuzzy, &template_catalog.entries[0]));
    mnd_merge_fuzzy_free(&fuzzy);

    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&reversed_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_search_later_better_candidate_wins)
{
    static const char old_po[] =
        "msgid \"abcdefgQQQQQ\"\n"
        "msgstr \"x\"\n"
        "\n"
        "msgid \"abcdeXgh\"\n"
        "msgstr \"y\"\n"
        "\n"
        "msgid \"abcdQQQQQQ\"\n"
        "msgstr \"z\"\n";
    static const char template_po[] =
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n";
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    /*
     * 数え上げは 0 が 4、1 が 2、2 が 1 で、0 から調べる。類似度は 0 が 14 / 20、
     * 1 が 14 / 16、2 は 8 / 18（しきい値以下）。後に調べた 1 が最良値を超えて残る
     */
    NAOS_CHECK_UINT_EQ(1, search(&fuzzy, &template_catalog.entries[0]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_short_query_examines_length_range_only)
{
    static const char old_po[] =
        "msgid \"a\"\n"
        "msgstr \"0\"\n"
        "\n"
        "msgid \"ab\"\n"
        "msgstr \"1\"\n"
        "\n"
        "msgid \"abcdefg\"\n"
        "msgstr \"2\"\n"
        "\n"
        "msgid \"abcdefgh\"\n"
        "msgstr \"3\"\n"
        "\n"
        "msgid \"xy\"\n"
        "msgstr \"4\"\n"
        "\n"
        "msgid \"abcd\"\n"
        "msgstr \"5\"\n"
        "\n"
        "msgid \"日\"\n"
        "msgstr \"6\"\n"
        "\n"
        "msgid \"aaaaaaaaaaaaaaaa\"\n"
        "msgstr \"7\"\n"
        "\n"
        "msgid \"aaaaaaaaaaaaaaaaa\"\n"
        "msgstr \"8\"\n";
    static const char template_po[] =
        "msgid \"abc\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"a日日\"\n"
        "msgstr \"\"\n";
    /*
     * "abc"（3 バイト）: k = 2 / 0.6 − 1 として ceil(3 / k) = 2 から (size_t)(3 × k) = 7 まで。
     * 長さ 2（1、4）、3（6）、4（5）、7（2）の順。長さ 1（0）、8（3）、16（7）は調べない
     */
    static const size_t expected_abc[] = { 1, 4, 6, 5, 2 };
    /*
     * "a日日"（7 バイト）: 7 / k は 3 をわずかに下回り、切り上げて 3。(size_t)(7 × k) = 16。
     * 長さ 3（6）、4（5）、7（2）、8（3）、16（7）の順。長さ 2 と 17（8）は調べない
     */
    static const size_t expected_mixed[] = { 6, 5, 2, 3, 7 };
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_UINT_EQ(7, template_catalog.entries[1].msgid.len);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    check_order(&fuzzy, &template_catalog.entries[0], expected_abc,
                sizeof(expected_abc) / sizeof(expected_abc[0]));
    check_order(&fuzzy, &template_catalog.entries[1], expected_mixed,
                sizeof(expected_mixed) / sizeof(expected_mixed[0]));

    /* "abc": "ab" は 4 / 5、"abcd" は 6 / 7（最良）、"abcdefg" は 6 / 10 + bonus */
    NAOS_CHECK_UINT_EQ(5, search(&fuzzy, &template_catalog.entries[0]));
    /* "a日日": "日" は 6 / 10 + bonus で 0.6 を超える（範囲の下端の長さ 3 を調べる） */
    NAOS_CHECK_UINT_EQ(6, search(&fuzzy, &template_catalog.entries[1]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_short_query_upper_length_boundary)
{
    static const char old_po[] =
        "msgid \"abcdefg\"\n"
        "msgstr \"x\"\n";
    static const char template_po[] =
        "msgid \"abc\"\n"
        "msgstr \"\"\n";
    static const size_t expected[] = { 0 };
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    /* 長さ 7 は範囲の上端。類似度 0.6 ちょうどに bonus を加えて選ぶ */
    check_order(&fuzzy, &template_catalog.entries[0], expected, 1);
    NAOS_CHECK_UINT_EQ(0, search(&fuzzy, &template_catalog.entries[0]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_short_japanese_query_uses_length_lists)
{
    static const char old_po[] =
        "msgid \"日本\"\n"
        "msgstr \"0\"\n"
        "\n"
        "msgid \"日本語だ\"\n"
        "msgstr \"1\"\n"
        "\n"
        "msgid \"日本語の文章です\"\n"
        "msgstr \"2\"\n"
        "\n"
        "msgid \"abc\"\n"
        "msgstr \"3\"\n";
    static const char template_po[] =
        "msgid \"日本語\"\n"
        "msgstr \"\"\n";
    /* 9 バイト: ceil(9 / k) = 4 から (size_t)(9 × k) = 21 まで。長さ 6（0）、12（1）の順 */
    static const size_t expected[] = { 0, 1 };
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_UINT_EQ(9, template_catalog.entries[0].msgid.len);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    check_order(&fuzzy, &template_catalog.entries[0], expected, 2);
    /* "日本" は 12 / 15、"日本語だ" は 18 / 21（最良） */
    NAOS_CHECK_UINT_EQ(1, search(&fuzzy, &template_catalog.entries[0]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_search_msgctxt_bonus)
{
    static const char old_po[] =
        "msgctxt \"other\"\n"
        "msgid \"abcdefgX\"\n"
        "msgstr \"0\"\n"
        "\n"
        "msgid \"abcdefgY\"\n"
        "msgstr \"1\"\n"
        "\n"
        "msgctxt \"ctx\"\n"
        "msgid \"abcdefgZ\"\n"
        "msgstr \"2\"\n"
        "\n"
        "msgctxt \"\"\n"
        "msgid \"abcdefgW\"\n"
        "msgstr \"3\"\n";
    static const char context_po[] =
        "msgctxt \"other\"\n"
        "msgid \"abcdefgX\"\n"
        "msgstr \"0\"\n"
        "\n"
        "msgctxt \"ctx\"\n"
        "msgid \"abcdefgY\"\n"
        "msgstr \"1\"\n"
        "\n"
        "msgctxt \"\"\n"
        "msgid \"abcdefgZ\"\n"
        "msgstr \"2\"\n"
        "\n"
        "msgctxt \"other\"\n"
        "msgid \"abcdefgh\"\n"
        "msgstr \"3\"\n";
    static const char template_po[] =
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgctxt \"ctx\"\n"
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgctxt \"other\"\n"
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgctxt \"\"\n"
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgctxt \"ctx\"\n"
        "msgid \"abcdefgV\"\n"
        "msgstr \"\"\n";
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t context_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(context_po, &context_catalog);
    TEST_READ_PO(template_po, &template_catalog);

    /* 候補 0〜3 の類似度はどれも 14 / 16、数え上げも同じ */
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));
    /* msgctxt のない問い合わせ: bonus は msgctxt のない候補 1 だけ */
    NAOS_CHECK_UINT_EQ(1, search(&fuzzy, &template_catalog.entries[0]));
    /* msgctxt "ctx": 候補 1（msgctxt なし）と 2（同じ）に bonus。先の 1 が残る */
    NAOS_CHECK_UINT_EQ(1, search(&fuzzy, &template_catalog.entries[1]));
    /* msgctxt "other": 候補 0 に bonus */
    NAOS_CHECK_UINT_EQ(0, search(&fuzzy, &template_catalog.entries[2]));
    mnd_merge_fuzzy_free(&fuzzy);

    /* msgctxt のない候補がない場合 */
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &context_catalog));
    /* msgctxt のない問い合わせ: どの候補にも bonus がなく、最も似た候補 3 */
    NAOS_CHECK_UINT_EQ(3, search(&fuzzy, &template_catalog.entries[0]));
    /* msgctxt "ctx": 同じ msgctxt の候補 1 が、より似た候補 3 には及ばない */
    NAOS_CHECK_UINT_EQ(3, search(&fuzzy, &template_catalog.entries[1]));
    /* msgctxt "ctx" で類似度が同じ候補だけなら、同じ msgctxt の候補 1 が先の 0 に勝つ */
    NAOS_CHECK_UINT_EQ(1, search(&fuzzy, &template_catalog.entries[4]));
    /* 空の msgctxt は空の msgctxt の候補 2 と等しい（候補 3 は msgid が同じで 1.0） */
    NAOS_CHECK_UINT_EQ(3, search(&fuzzy, &template_catalog.entries[3]));
    mnd_merge_fuzzy_free(&fuzzy);

    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&context_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_search_empty_msgctxt_bonus)
{
    static const char old_po[] =
        "msgctxt \"x\"\n"
        "msgid \"abcdefgX\"\n"
        "msgstr \"0\"\n"
        "\n"
        "msgctxt \"\"\n"
        "msgid \"abcdefgY\"\n"
        "msgstr \"1\"\n";
    static const char template_po[] =
        "msgctxt \"\"\n"
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgctxt \"x\"\n"
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n";
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    /* 空の msgctxt は空の msgctxt とだけ等しい */
    NAOS_CHECK_UINT_EQ(1, search(&fuzzy, &template_catalog.entries[0]));
    NAOS_CHECK_UINT_EQ(0, search(&fuzzy, &template_catalog.entries[1]));
    /* msgctxt のない問い合わせは、空の msgctxt の候補と等しくない（先の候補が残る） */
    NAOS_CHECK_UINT_EQ(0, search(&fuzzy, &template_catalog.entries[2]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_search_uses_obsolete_but_not_untranslated_or_header)
{
    static const char old_po[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "#~ msgid \"abcdefgX\"\n"
        "#~ msgstr \"x\"\n";
    static const char template_po[] =
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"\"\n"
        "msgstr \"\"\n";
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    /* 未翻訳の同じ msgid（1）ではなく、訳のある廃止エントリ（2）を選ぶ */
    NAOS_CHECK_UINT_EQ(2, search(&fuzzy, &template_catalog.entries[0]));
    /* 空の msgid の問い合わせでも、ヘッダ（0）は候補にならない */
    NAOS_CHECK_UINT_EQ(FUZZY_TEST_NOT_FOUND, search(&fuzzy, &template_catalog.entries[1]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_search_without_candidates)
{
    static const char template_po[] =
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"ab\"\n"
        "msgstr \"\"\n";
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;

    mnd_po_catalog_init(&old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    check_order(&fuzzy, &template_catalog.entries[0], NULL, 0);
    NAOS_CHECK_UINT_EQ(FUZZY_TEST_NOT_FOUND, search(&fuzzy, &template_catalog.entries[0]));
    NAOS_CHECK_UINT_EQ(FUZZY_TEST_NOT_FOUND, search(&fuzzy, &template_catalog.entries[1]));

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_TEST(fuzzy_search_repeated_gives_same_result)
{
    static const char old_po[] =
        "msgid \"abcdeXgh\"\n"
        "msgstr \"0\"\n"
        "\n"
        "msgid \"abcdefgX\"\n"
        "msgstr \"1\"\n"
        "\n"
        "msgid \"日本語だ\"\n"
        "msgstr \"2\"\n"
        "\n"
        "msgid \"The quick brown fox jumps over the lazy dog, and then some more text follows "
        "so that the query spans more than one 64-bit word of the mask.\"\n"
        "msgstr \"3\"\n";
    static const char template_po[] =
        "msgid \"abcdefgh\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"日本語\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"The quick brown fox jumped over the lazy dog, and then some more text follows "
        "so that the query spans more than one 64-bit word of the mask!\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"zzzzzzzz\"\n"
        "msgstr \"\"\n";
    static const size_t expected[] = { 1, 2, 3, FUZZY_TEST_NOT_FOUND };
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t template_catalog;
    mnd_merge_fuzzy_t fuzzy;
    size_t round;
    size_t i;

    TEST_READ_PO(old_po, &old_catalog);
    TEST_READ_PO(template_po, &template_catalog);
    NAOS_REQUIRE_UINT_EQ(4, template_catalog.count);
    NAOS_REQUIRE_TRUE(mnd_merge_fuzzy_init(&fuzzy, &old_catalog));

    /* 長い問い合わせ・短い問い合わせを交互に繰り返しても、毎回同じ結果になる */
    for (round = 0; round < 3; round++)
    {
        for (i = 0; i < template_catalog.count; i++)
        {
            NAOS_CHECK_UINT_EQ(expected[i], search(&fuzzy, &template_catalog.entries[i]));
        }
    }

    mnd_merge_fuzzy_free(&fuzzy);
    mnd_po_catalog_free(&template_catalog);
    mnd_po_catalog_free(&old_catalog);
}

NAOS_SUITE(fuzzy,
    NAOS_ADD_TEST(fuzzy_scaffold_has_length_list_per_short_byte_length),
    NAOS_ADD_TEST(fuzzy_init_registers_translated_entries_only),
    NAOS_ADD_TEST(fuzzy_init_lists_repeated_gram_once),
    NAOS_ADD_TEST(fuzzy_init_grams_are_utf8_characters),
    NAOS_ADD_TEST(fuzzy_init_length_list_boundary),
    NAOS_ADD_TEST(fuzzy_init_empty_catalog),
    NAOS_ADD_TEST(fuzzy_search_does_not_choose_similarity_exactly_threshold),
    NAOS_ADD_TEST(fuzzy_search_ignores_candidates_without_shared_gram),
    NAOS_ADD_TEST(fuzzy_order_sorts_by_count_then_candidate_number),
    NAOS_ADD_TEST(fuzzy_search_tie_prefers_higher_count),
    NAOS_ADD_TEST(fuzzy_search_tie_with_same_count_prefers_earlier),
    NAOS_ADD_TEST(fuzzy_search_later_better_candidate_wins),
    NAOS_ADD_TEST(fuzzy_short_query_examines_length_range_only),
    NAOS_ADD_TEST(fuzzy_short_query_upper_length_boundary),
    NAOS_ADD_TEST(fuzzy_short_japanese_query_uses_length_lists),
    NAOS_ADD_TEST(fuzzy_search_msgctxt_bonus),
    NAOS_ADD_TEST(fuzzy_search_empty_msgctxt_bonus),
    NAOS_ADD_TEST(fuzzy_search_uses_obsolete_but_not_untranslated_or_header),
    NAOS_ADD_TEST(fuzzy_search_without_candidates),
    NAOS_ADD_TEST(fuzzy_search_repeated_gives_same_result)
)
