/* スイート path: キーパスによる照合（4.1〜4.8、6.7、6.9） */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "foundation/source.h"
#include "test_alloc.h"
#include "yaml/yaml.h"
#include "yaml/yaml_parse.h"
#include "yaml/yaml_path.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * data[0..len) 全体を解析した文書を返す（呼び出し側が解放する）。allocator は NULL 可。失敗したら NULL
 */
static mnd_yaml_doc_t *parse_len(const char *data, size_t len, const mnd_yaml_allocator_t *allocator)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_options_t options = MND_YAML_OPTIONS_INIT;
    mnd_yaml_doc_t *doc = NULL;
    mnd_span_t content;
    bool ok;

    content.start = 0;
    content.end = len;
    options.allocator = allocator;
    ok = mnd_yaml_parse(data, len, content, &options, &doc, &diags);
    NAOS_CHECK_TRUE(ok);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
    if (!ok)
    {
        mnd_yaml_doc_free(doc);
        return NULL;
    }

    return doc;
}

static mnd_yaml_doc_t *parse_text(const char *text)
{
    return parse_len(text, strlen(text), NULL);
}

/*
 * path の照合が成功し、診断が増えないことを確かめて結果を返す（呼び出し側が mnd_yaml_matches_free で解放する）
 */
static mnd_yaml_matches_t find_ok(const mnd_yaml_doc_t *doc, const char *path)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_matches_t matches = { NULL, 0 };

    NAOS_CHECK_TRUE(mnd_yaml_path_find(doc, path, strlen(path), &matches, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);

    return matches;
}

/*
 * text の path の照合が成功し、該当するノードがすべてスカラーで、論理値が順に values[0..count) で
 * あることを確かめる
 */
static void check_values(const char *text, const char *path, const char *const *values, size_t count)
{
    mnd_yaml_doc_t *doc = parse_text(text);
    mnd_yaml_matches_t matches;
    size_t i;

    NAOS_REQUIRE_TRUE(doc != NULL);
    matches = find_ok(doc, path);
    NAOS_CHECK_UINT_EQ(count, matches.count);
    if (matches.count == count)
    {
        for (i = 0; i < count; ++i)
        {
            NAOS_CHECK_INT_EQ((int)MND_YAML_SCALAR, (int)matches.nodes[i]->kind);
            if (matches.nodes[i]->kind == MND_YAML_SCALAR)
            {
                NAOS_CHECK_STR_EQ(values[i], matches.nodes[i]->as.scalar.value);
            }
        }
    }
    if (count == 0)
    {
        NAOS_CHECK_TRUE(matches.nodes == NULL);
    }
    mnd_yaml_matches_free(doc, &matches);
    NAOS_CHECK_TRUE(matches.nodes == NULL);
    NAOS_CHECK_UINT_EQ(0, matches.count);
    mnd_yaml_doc_free(doc);
}

/*
 * path[0..path_len) の照合が失敗し、*out が {NULL, 0} になり、診断が 1 つ（エラー、message、行番号 0、
 * パスなし）だけ追加されることを確かめる
 */
static void check_find_error(const mnd_yaml_doc_t *doc, const char *path, size_t path_len, const char *message)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_matches_t matches;
    const mnd_diag_t *d = NULL;

    matches.nodes = (const mnd_yaml_node_t **)&diags;
    matches.count = 7;
    NAOS_CHECK_FALSE(mnd_yaml_path_find(doc, path, path_len, &matches, &diags));
    NAOS_CHECK_TRUE(matches.nodes == NULL);
    NAOS_CHECK_UINT_EQ(0, matches.count);
    NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&diags));
    if (mnd_diag_list_count(&diags) == 1)
    {
        d = mnd_diag_list_at(&diags, 0);
        NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)d->severity);
        NAOS_CHECK_STR_EQ(message, d->message);
        NAOS_CHECK_NULL(d->path);
        NAOS_CHECK_UINT_EQ(0, d->line);
    }
    mnd_diag_list_free(&diags);
}

static const char front_matter_text[] =
    "title: Hello\n"
    "seo:\n"
    "  title: \"SEO title\"\n"
    "  description: d\n"
    "tags:\n"
    "  - a\n"
    "  - 'b'\n"
    "  - c\n"
    "authors:\n"
    "  - name: Alice\n"
    "    email: alice@example.com\n"
    "  - name: Bob\n"
    "  - plain\n"
    "  - {name: Carol}\n";

/* 4.1: `.` で区切った各セグメントをルートのマッピングから順に照合する */
NAOS_TEST(path_finds_top_level_and_nested_keys)
{
    static const char *const title[] = { "Hello" };
    static const char *const seo_title[] = { "SEO title" };

    check_values(front_matter_text, "title", title, 1);
    check_values(front_matter_text, "seo.title", seo_title, 1);
}

/* 4.2: シーケンスは途中でも末尾でも全要素に照合を続ける */
NAOS_TEST(path_applies_to_all_sequence_items)
{
    static const char *const tags[] = { "a", "b", "c" };
    static const char *const names[] = { "Alice", "Bob", "Carol" };
    static const char *const nested[] = { "x", "y", "z", "w" };

    check_values(front_matter_text, "tags", tags, 3);
    check_values(front_matter_text, "authors.name", names, 3);
    check_values("t: [x, [y, [z]], w]\n", "t", nested, 4);
}

/* 4.3: 原文の出現順に、論理値・範囲・行番号・スタイルとともに返す */
NAOS_TEST(path_returns_scalars_in_source_order_with_details)
{
    const char *text =
        "x:\n"
        "  - y: one\n"
        "  - y: ['two', \"three\"]\n"
        "  - z: zero\n"
        "  - y: four\n";
    mnd_yaml_doc_t *doc = parse_text(text);
    mnd_yaml_matches_t matches;
    const mnd_yaml_node_t *n = NULL;
    size_t i;

    NAOS_REQUIRE_TRUE(doc != NULL);
    matches = find_ok(doc, "x.y");
    NAOS_REQUIRE_UINT_EQ(4, matches.count);
    for (i = 0; i < 4; ++i)
    {
        NAOS_CHECK_INT_EQ((int)MND_YAML_SCALAR, (int)matches.nodes[i]->kind);
    }
    for (i = 1; i < 4; ++i)
    {
        NAOS_CHECK_UINT_GT(matches.nodes[i]->span.start, matches.nodes[i - 1]->span.start);
    }

    n = matches.nodes[0];
    NAOS_CHECK_STR_EQ("one", n->as.scalar.value);
    NAOS_CHECK_INT_EQ((int)MND_YAML_PLAIN, (int)n->as.scalar.style);
    NAOS_CHECK_UINT_EQ(10, n->span.start);
    NAOS_CHECK_UINT_EQ(13, n->span.end);
    NAOS_CHECK_UINT_EQ(2, n->start_line);

    n = matches.nodes[1];
    NAOS_CHECK_STR_EQ("two", n->as.scalar.value);
    NAOS_CHECK_INT_EQ((int)MND_YAML_SINGLE_QUOTED, (int)n->as.scalar.style);
    NAOS_CHECK_MEM_EQ("'two'", 5, text + n->span.start, n->span.end - n->span.start);
    NAOS_CHECK_UINT_EQ(5, n->span.end - n->span.start);
    NAOS_CHECK_UINT_EQ(3, n->start_line);

    n = matches.nodes[2];
    NAOS_CHECK_STR_EQ("three", n->as.scalar.value);
    NAOS_CHECK_INT_EQ((int)MND_YAML_DOUBLE_QUOTED, (int)n->as.scalar.style);
    NAOS_CHECK_MEM_EQ("\"three\"", 7, text + n->span.start, n->span.end - n->span.start);
    NAOS_CHECK_UINT_EQ(3, n->start_line);

    n = matches.nodes[3];
    NAOS_CHECK_STR_EQ("four", n->as.scalar.value);
    NAOS_CHECK_UINT_EQ(5, n->start_line);

    mnd_yaml_matches_free(doc, &matches);
    mnd_yaml_doc_free(doc);
}

/* 4.4: 該当なしは成功で count == 0 */
NAOS_TEST(path_no_match_is_success_with_empty_result)
{
    mnd_yaml_doc_t *doc = NULL;
    mnd_yaml_matches_t matches;

    check_values(front_matter_text, "missing", NULL, 0);
    check_values(front_matter_text, "seo.missing", NULL, 0);
    check_values(front_matter_text, "title.x", NULL, 0);
    check_values(front_matter_text, "tags.x", NULL, 0);
    check_values(front_matter_text, "authors.email.x", NULL, 0);
    /* ルートがマッピングでない */
    check_values("hello\n", "hello", NULL, 0);
    check_values("- title: a\n", "title", NULL, 0);
    /* 空のシーケンスと null の値 */
    check_values("a: []\nb:\n", "a", NULL, 0);

    /* 空のドキュメント（根が NULL） */
    doc = parse_text("# only a comment\n");
    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_NULL(mnd_yaml_doc_root(doc));
    matches = find_ok(doc, "title");
    NAOS_CHECK_UINT_EQ(0, matches.count);
    NAOS_CHECK_TRUE(matches.nodes == NULL);
    mnd_yaml_matches_free(doc, &matches);
    mnd_yaml_doc_free(doc);
}

/* 4.5: マッピングに当たった場合は kind で区別できる */
NAOS_TEST(path_match_on_mapping_is_distinguishable)
{
    mnd_yaml_doc_t *doc = parse_text(front_matter_text);
    mnd_yaml_matches_t matches;

    NAOS_REQUIRE_TRUE(doc != NULL);
    matches = find_ok(doc, "seo");
    NAOS_REQUIRE_UINT_EQ(1, matches.count);
    NAOS_CHECK_INT_EQ((int)MND_YAML_MAPPING, (int)matches.nodes[0]->kind);
    NAOS_CHECK_UINT_EQ(2, matches.nodes[0]->as.mapping.count);
    mnd_yaml_matches_free(doc, &matches);

    /* シーケンスの要素がマッピングとスカラーの混在なら、要素ごとに kind で区別できる */
    matches = find_ok(doc, "authors");
    NAOS_REQUIRE_UINT_EQ(4, matches.count);
    NAOS_CHECK_INT_EQ((int)MND_YAML_MAPPING, (int)matches.nodes[0]->kind);
    NAOS_CHECK_INT_EQ((int)MND_YAML_MAPPING, (int)matches.nodes[1]->kind);
    NAOS_CHECK_INT_EQ((int)MND_YAML_SCALAR, (int)matches.nodes[2]->kind);
    NAOS_CHECK_STR_EQ("plain", matches.nodes[2]->as.scalar.value);
    NAOS_CHECK_INT_EQ((int)MND_YAML_MAPPING, (int)matches.nodes[3]->kind);
    mnd_yaml_matches_free(doc, &matches);
    mnd_yaml_doc_free(doc);
}

/* 4.6: キーの論理値の完全一致（大文字と小文字を区別し、引用符の有無を問わない） */
NAOS_TEST(path_matches_logical_key_value_exactly)
{
    static const char *const a[] = { "A" };
    static const char *const b[] = { "B" };
    static const char *const s[] = { "S" };

    check_values("Title: A\n", "title", NULL, 0);
    check_values("Title: A\n", "Title", a, 1);
    check_values("\"Title\": B\n", "title", NULL, 0);
    check_values("\"Title\": B\n", "Title", b, 1);
    check_values("'seo':\n  \"title\": S\n", "seo.title", s, 1);
    check_values("{'seo': {\"ti\\x74le\": S}}\n", "seo.title", s, 1);
    /* 前方一致や部分一致では当たらない */
    check_values("titles: A\ntitl: B\n", "title", NULL, 0);
}

/* 4.7: 空のキーパスと空のセグメントはエラー */
NAOS_TEST(path_rejects_empty_path_and_empty_segments)
{
    mnd_yaml_doc_t *doc = parse_text(front_matter_text);

    NAOS_REQUIRE_TRUE(doc != NULL);
    check_find_error(doc, "", 0, "invalid key path \"\"");
    check_find_error(doc, NULL, 0, "invalid key path \"\"");
    check_find_error(doc, "a..b", 4, "invalid key path \"a..b\"");
    check_find_error(doc, ".a", 2, "invalid key path \".a\"");
    check_find_error(doc, "a.", 2, "invalid key path \"a.\"");
    check_find_error(doc, ".", 1, "invalid key path \".\"");
    /* path_len までだけを使う */
    check_find_error(doc, "seo..title", 4, "invalid key path \"seo.\"");
    /* 該当があるキーでも、不正なキーパスなら照合しない */
    check_find_error(doc, "title.", 6, "invalid key path \"title.\"");
    mnd_yaml_doc_free(doc);
}

/* 4.8: `.` を含むキーはキーパスで指定できない */
NAOS_TEST(path_key_containing_dot_is_not_matched)
{
    static const char *const z[] = { "z" };

    check_values("a.b: x\n", "a.b", NULL, 0);
    check_values("\"a.b\": x\n", "a.b", NULL, 0);
    check_values("\"a.b\": x\na:\n  b: z\n", "a.b", z, 1);
}

/* path_len で区切った範囲だけを照合する */
NAOS_TEST(path_uses_only_path_len_bytes)
{
    mnd_yaml_doc_t *doc = parse_text(front_matter_text);
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_matches_t matches = { NULL, 0 };

    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_TRUE(mnd_yaml_path_find(doc, "seo.titleXYZ", 9, &matches, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_REQUIRE_UINT_EQ(1, matches.count);
    NAOS_CHECK_STR_EQ("SEO title", matches.nodes[0]->as.scalar.value);
    mnd_yaml_matches_free(doc, &matches);
    /* 解放済みに対しても安全 */
    mnd_yaml_matches_free(doc, &matches);
    NAOS_CHECK_TRUE(matches.nodes == NULL);
    mnd_diag_list_free(&diags);
    mnd_yaml_doc_free(doc);
}

/*
 * 6.9: 10,000 段の入れ子での照合が異常終了しない。
 * フロー形式のシーケンスの入れ子を末尾で展開する場合と、マッピングの入れ子を 10,000 個のセグメントで
 * たどる場合を確かめる
 */
NAOS_TEST(path_deep_nesting_does_not_crash)
{
    const size_t depth = 10000;
    char *data = NULL;
    char *path = NULL;
    size_t len = 0;
    size_t path_len = 0;
    size_t i;
    mnd_yaml_doc_t *doc = NULL;
    mnd_yaml_matches_t matches;

    /* "a: " + "[" * depth + "x" + "]" * depth */
    data = (char *)malloc((depth * 5) + 16);
    NAOS_REQUIRE_TRUE(data != NULL);
    path = (char *)malloc((depth * 2) + 1);
    if (path == NULL)
    {
        free(data);
        NAOS_REQUIRE_TRUE(path != NULL);
    }

    memcpy(data, "a: ", 3);
    len = 3;
    for (i = 0; i < depth; ++i)
    {
        data[len++] = '[';
    }
    data[len++] = 'x';
    for (i = 0; i < depth; ++i)
    {
        data[len++] = ']';
    }
    data[len++] = '\n';
    doc = parse_len(data, len, NULL);
    NAOS_CHECK_TRUE(doc != NULL);
    if (doc != NULL)
    {
        matches = find_ok(doc, "a");
        NAOS_CHECK_UINT_EQ(1, matches.count);
        if (matches.count == 1)
        {
            NAOS_CHECK_STR_EQ("x", matches.nodes[0]->as.scalar.value);
        }
        mnd_yaml_matches_free(doc, &matches);
        mnd_yaml_doc_free(doc);
    }

    /* "{a: " * depth + "x" + "}" * depth と "a.a. ... .a"（depth 個のセグメント） */
    len = 0;
    for (i = 0; i < depth; ++i)
    {
        memcpy(data + len, "{a: ", 4);
        len += 4;
    }
    data[len++] = 'x';
    for (i = 0; i < depth; ++i)
    {
        data[len++] = '}';
    }
    data[len++] = '\n';
    for (i = 0; i < depth; ++i)
    {
        if (i > 0)
        {
            path[path_len++] = '.';
        }
        path[path_len++] = 'a';
    }
    path[path_len] = '\0';
    doc = parse_len(data, len, NULL);
    NAOS_CHECK_TRUE(doc != NULL);
    if (doc != NULL)
    {
        matches = find_ok(doc, path);
        NAOS_CHECK_UINT_EQ(1, matches.count);
        if (matches.count == 1)
        {
            NAOS_CHECK_STR_EQ("x", matches.nodes[0]->as.scalar.value);
        }
        mnd_yaml_matches_free(doc, &matches);
        /* 1 段足りないキーパスはマッピングに当たる */
        matches = find_ok(doc, "a.a.a");
        NAOS_CHECK_UINT_EQ(1, matches.count);
        if (matches.count == 1)
        {
            NAOS_CHECK_INT_EQ((int)MND_YAML_MAPPING, (int)matches.nodes[0]->kind);
        }
        mnd_yaml_matches_free(doc, &matches);
        mnd_yaml_doc_free(doc);
    }
    free(path);
    free(data);
}

/*
 * 6.7: 照合の確保が失敗すると false と out of memory（行番号 0）を返し、*out を {NULL, 0} にし、
 * 確保が残らない。作業用の配列と結果の配列が何度も伸びる入力を使う
 */
NAOS_TEST(path_out_of_memory)
{
    const size_t items = 30;
    const size_t values = 40;
    mnd_yaml_test_alloc_t ta;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_yaml_matches_t matches;
    mnd_yaml_doc_t *doc = NULL;
    const mnd_diag_t *d = NULL;
    char *data = NULL;
    size_t len = 0;
    size_t live;
    size_t n;
    size_t i;
    size_t j;
    bool ok = false;

    /* "x:\n" と items 個の "  - y: [[[v], v], v, ...]\n"（入れ子のシーケンスでスタックも伸ばす） */
    data = (char *)malloc(3 + (items * (16 + (values * 4))));
    NAOS_REQUIRE_TRUE(data != NULL);
    memcpy(data, "x:\n", 3);
    len = 3;
    for (i = 0; i < items; ++i)
    {
        memcpy(data + len, "  - y: [[[v], v], ", 18);
        len += 18;
        for (j = 0; j < values; ++j)
        {
            memcpy(data + len, "v, ", 3);
            len += 3;
        }
        memcpy(data + len, "v]\n", 3);
        len += 3;
    }

    mnd_yaml_test_alloc_init(&ta, 0);
    doc = parse_len(data, len, &ta.allocator);
    if (doc == NULL)
    {
        free(data);
        NAOS_REQUIRE_TRUE(doc != NULL);
    }
    live = mnd_yaml_test_alloc_live(&ta);

    for (n = 1; n < 1000; ++n)
    {
        ta.fail_at = ta.calls + n;
        ta.failures = 0;
        matches.nodes = (const mnd_yaml_node_t **)&diags;
        matches.count = 7;
        ok = mnd_yaml_path_find(doc, "x.y", 3, &matches, &diags);
        if (ok)
        {
            NAOS_CHECK_UINT_EQ(0, ta.failures);
            NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
            break;
        }
        NAOS_CHECK_UINT_EQ(1, ta.failures);
        NAOS_CHECK_TRUE(matches.nodes == NULL);
        NAOS_CHECK_UINT_EQ(0, matches.count);
        NAOS_CHECK_UINT_EQ(1, mnd_diag_list_count(&diags));
        if (mnd_diag_list_count(&diags) == 1)
        {
            d = mnd_diag_list_at(&diags, 0);
            NAOS_CHECK_INT_EQ((int)MND_SEVERITY_ERROR, (int)d->severity);
            NAOS_CHECK_STR_EQ("out of memory", d->message);
            NAOS_CHECK_UINT_EQ(0, d->line);
        }
        mnd_diag_list_free(&diags);
        NAOS_CHECK_UINT_EQ(live, mnd_yaml_test_alloc_live(&ta));
    }
    NAOS_CHECK_TRUE(ok);
    /* 作業用の配列が伸びる経路を通るよう、少なくとも数回は失敗させている */
    NAOS_CHECK_UINT_GT(n, 4);
    if (ok)
    {
        NAOS_CHECK_UINT_EQ(items * (values + 3), matches.count);
        NAOS_CHECK_UINT_EQ(live + 1, mnd_yaml_test_alloc_live(&ta));
        mnd_yaml_matches_free(doc, &matches);
        NAOS_CHECK_TRUE(matches.nodes == NULL);
    }
    NAOS_CHECK_UINT_EQ(live, mnd_yaml_test_alloc_live(&ta));
    mnd_diag_list_free(&diags);
    mnd_yaml_doc_free(doc);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
    free(data);
}

NAOS_SUITE(path,
    NAOS_ADD_TEST(path_finds_top_level_and_nested_keys),
    NAOS_ADD_TEST(path_applies_to_all_sequence_items),
    NAOS_ADD_TEST(path_returns_scalars_in_source_order_with_details),
    NAOS_ADD_TEST(path_no_match_is_success_with_empty_result),
    NAOS_ADD_TEST(path_match_on_mapping_is_distinguishable),
    NAOS_ADD_TEST(path_matches_logical_key_value_exactly),
    NAOS_ADD_TEST(path_rejects_empty_path_and_empty_segments),
    NAOS_ADD_TEST(path_key_containing_dot_is_not_matched),
    NAOS_ADD_TEST(path_uses_only_path_len_bytes),
    NAOS_ADD_TEST(path_deep_nesting_does_not_crash),
    NAOS_ADD_TEST(path_out_of_memory)
)
