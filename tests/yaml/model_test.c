/* スイート model: 構文木の型、アリーナ、文書のアクセサと解放（2.6、3.1、6.7、6.10）と、テスト用の確保関数 */

#include <naos/naos.h>

#include "test_alloc.h"
#include "tree_check.h"
#include "yaml/yaml.h"
#include "yaml/yaml_internal.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* test_alloc */

NAOS_TEST(test_alloc_counts_allocs_and_frees)
{
    mnd_yaml_test_alloc_t ta;
    void *a = NULL;
    void *b = NULL;

    mnd_yaml_test_alloc_init(&ta, 0);
    a = ta.allocator.alloc(ta.allocator.ctx, 16);
    b = ta.allocator.alloc(ta.allocator.ctx, 0);
    NAOS_REQUIRE_TRUE(a != NULL);
    NAOS_REQUIRE_TRUE(b != NULL);
    NAOS_CHECK_UINT_EQ(2, ta.calls);
    NAOS_CHECK_UINT_EQ(2, ta.allocs);
    NAOS_CHECK_UINT_EQ(2, mnd_yaml_test_alloc_live(&ta));
    ta.allocator.free(ta.allocator.ctx, a);
    ta.allocator.free(ta.allocator.ctx, NULL); /* NULL は数えない */
    ta.allocator.free(ta.allocator.ctx, b);
    NAOS_CHECK_UINT_EQ(2, ta.frees);
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));
    NAOS_CHECK_UINT_EQ(0, ta.failures);
}

NAOS_TEST(test_alloc_fails_only_at_nth_alloc)
{
    mnd_yaml_test_alloc_t ta;
    void *first = NULL;
    void *second = NULL;
    void *third = NULL;

    mnd_yaml_test_alloc_init(&ta, 2);
    first = ta.allocator.alloc(ta.allocator.ctx, 8);
    second = ta.allocator.alloc(ta.allocator.ctx, 8);
    third = ta.allocator.alloc(ta.allocator.ctx, 8);
    NAOS_CHECK_TRUE(first != NULL);
    NAOS_CHECK_NULL(second);
    NAOS_CHECK_TRUE(third != NULL);
    NAOS_CHECK_UINT_EQ(3, ta.calls);
    NAOS_CHECK_UINT_EQ(2, ta.allocs);
    NAOS_CHECK_UINT_EQ(1, ta.failures);
    ta.allocator.free(ta.allocator.ctx, first);
    ta.allocator.free(ta.allocator.ctx, third);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
}

/* 雛形: 確保関数の既定値と容量の計算 */
NAOS_TEST(model_scaffold_default_allocator_and_size_mul)
{
    const mnd_yaml_allocator_t *a = mnd_yaml_allocator_or_default(NULL);
    mnd_yaml_test_alloc_t ta;
    void *ptr = NULL;
    size_t size = 7;

    NAOS_REQUIRE_TRUE(a != NULL);
    ptr = a->alloc(a->ctx, 0);
    NAOS_CHECK_TRUE(ptr != NULL);
    a->free(a->ctx, ptr);
    a->free(a->ctx, NULL);
    mnd_yaml_test_alloc_init(&ta, 0);
    NAOS_CHECK_PTR_EQ(&ta.allocator, mnd_yaml_allocator_or_default(&ta.allocator));

    NAOS_CHECK_TRUE(mnd_yaml_size_mul(3, 5, &size));
    NAOS_CHECK_UINT_EQ(15, size);
    NAOS_CHECK_TRUE(mnd_yaml_size_mul(SIZE_MAX, 0, &size));
    NAOS_CHECK_UINT_EQ(0, size);
    size = 7;
    NAOS_CHECK_FALSE(mnd_yaml_size_mul((SIZE_MAX / 2) + 1, 2, &size));
    NAOS_CHECK_UINT_EQ(7, size);
}


/* 文書の作成とアクセサ */

NAOS_TEST(model_doc_new_empty_and_accessors)
{
    static const char text[] = "---\nk: v\n---\n";
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_doc_t *doc = NULL;
    mnd_span_t content;
    mnd_span_t got;

    content.start = 4;
    content.end = 9;
    mnd_yaml_test_alloc_init(&ta, 0);
    doc = mnd_yaml_doc_new(text, sizeof(text) - 1, content, MND_EOL_NONE, &ta.allocator);
    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_NULL(mnd_yaml_doc_root(doc));
    NAOS_CHECK_PTR_EQ(text, mnd_yaml_doc_data(doc));
    NAOS_CHECK_UINT_EQ(sizeof(text) - 1, mnd_yaml_doc_len(doc));
    got = mnd_yaml_doc_content(doc);
    NAOS_CHECK_UINT_EQ(4, got.start);
    NAOS_CHECK_UINT_EQ(9, got.end);
    /* MND_EOL_NONE は LF として持つ */
    NAOS_CHECK_INT_EQ((int)MND_EOL_LF, (int)mnd_yaml_doc_eol(doc));
    /* 空のドキュメントは検査を満たす */
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(doc));
    mnd_yaml_doc_free(doc);
    NAOS_CHECK_UINT_GE(ta.allocs, 1);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
}

NAOS_TEST(model_doc_new_keeps_crlf_and_accepts_null_data)
{
    static const char text[] = "a: b\r\n";
    mnd_yaml_doc_t *doc = NULL;
    mnd_span_t content;

    content.start = 0;
    content.end = sizeof(text) - 1;
    doc = mnd_yaml_doc_new(text, sizeof(text) - 1, content, MND_EOL_CRLF, NULL);
    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_INT_EQ((int)MND_EOL_CRLF, (int)mnd_yaml_doc_eol(doc));
    mnd_yaml_doc_free(doc);

    content.end = 0;
    doc = mnd_yaml_doc_new(NULL, 0, content, MND_EOL_LF, NULL);
    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_NULL(mnd_yaml_doc_data(doc));
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_doc_len(doc));
    NAOS_CHECK_NULL(mnd_yaml_doc_root(doc));
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(doc));
    mnd_yaml_doc_free(doc);
    /* NULL に対しても安全 */
    mnd_yaml_doc_free(NULL);
}

NAOS_TEST(model_doc_new_returns_null_on_alloc_failure)
{
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_doc_t *doc = NULL;
    mnd_span_t content;

    content.start = 0;
    content.end = 0;
    mnd_yaml_test_alloc_init(&ta, 1);
    doc = mnd_yaml_doc_new(NULL, 0, content, MND_EOL_LF, &ta.allocator);
    NAOS_CHECK_NULL(doc);
    NAOS_CHECK_UINT_EQ(1, ta.failures);
    NAOS_CHECK_UINT_EQ(0, mnd_yaml_test_alloc_live(&ta));
    mnd_yaml_doc_free(doc);
}

NAOS_TEST(model_arena_returns_null_on_alloc_failure)
{
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_doc_t *doc = NULL;
    mnd_span_t content;

    content.start = 0;
    content.end = 0;
    /* 1 回目は文書の構造体。2 回目（最初の塊）で失敗させる */
    mnd_yaml_test_alloc_init(&ta, 2);
    doc = mnd_yaml_doc_new(NULL, 0, content, MND_EOL_LF, &ta.allocator);
    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_NULL(mnd_yaml_node_new(doc, MND_YAML_SCALAR));
    NAOS_CHECK_UINT_EQ(1, ta.failures);
    /* 失敗のあとは確保できる */
    NAOS_CHECK_TRUE(mnd_yaml_node_new(doc, MND_YAML_SCALAR) != NULL);

    ta.fail_at = ta.calls + 1;
    NAOS_CHECK_NULL(mnd_yaml_arena_alloc(doc, 100000));
    NAOS_CHECK_UINT_EQ(2, ta.failures);
    /* 大きさのオーバーフロー */
    NAOS_CHECK_NULL(mnd_yaml_arena_alloc(doc, SIZE_MAX));
    NAOS_CHECK_NULL(mnd_yaml_arena_strdup(doc, "abc", SIZE_MAX));
    mnd_yaml_doc_free(doc);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);

    /* 新しい文書の最初の塊で失敗させる（論理値の複製） */
    mnd_yaml_test_alloc_init(&ta, 2);
    doc = mnd_yaml_doc_new(NULL, 0, content, MND_EOL_LF, &ta.allocator);
    NAOS_REQUIRE_TRUE(doc != NULL);
    NAOS_CHECK_NULL(mnd_yaml_arena_strdup(doc, "abc", 3));
    NAOS_CHECK_UINT_EQ(1, ta.failures);
    mnd_yaml_doc_free(doc);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
}

/* アリーナ */

NAOS_TEST(model_arena_alloc_is_zeroed_aligned_and_freed_at_once)
{
    mnd_yaml_test_alloc_t ta;
    mnd_yaml_doc_t *doc = NULL;
    mnd_span_t content;
    unsigned char *prev = NULL;
    unsigned char *big = NULL;
    void *empty = NULL;
    size_t i;
    size_t j;
    bool zeroed = true;
    bool aligned = true;
    bool distinct = true;

    content.start = 0;
    content.end = 0;
    mnd_yaml_test_alloc_init(&ta, 0);
    doc = mnd_yaml_doc_new(NULL, 0, content, MND_EOL_LF, &ta.allocator);
    NAOS_REQUIRE_TRUE(doc != NULL);

    empty = mnd_yaml_arena_alloc(doc, 0);
    NAOS_CHECK_TRUE(empty != NULL);

    /* 複数の塊にまたがる数の小さな確保 */
    for (i = 0; i < 2000; ++i)
    {
        unsigned char *p = (unsigned char *)mnd_yaml_arena_alloc(doc, 1 + (i % 37));

        if (p == NULL)
        {
            zeroed = false;
            break;
        }
        for (j = 0; j < (1 + (i % 37)); ++j)
        {
            if (p[j] != 0)
            {
                zeroed = false;
            }
        }
        if (((uintptr_t)p % sizeof(double)) != 0)
        {
            aligned = false;
        }
        if ((prev != NULL) && (p == prev))
        {
            distinct = false;
        }
        /* 前の確保に書き込んでも、次の確保は 0 で埋まっている */
        memset(p, 0xAB, 1 + (i % 37));
        prev = p;
    }
    NAOS_CHECK_TRUE(zeroed);
    NAOS_CHECK_TRUE(aligned);
    NAOS_CHECK_TRUE(distinct);

    /* 塊より大きな確保 */
    big = (unsigned char *)mnd_yaml_arena_alloc(doc, 100000);
    NAOS_REQUIRE_TRUE(big != NULL);
    NAOS_CHECK_UINT_EQ(0, big[0]);
    NAOS_CHECK_UINT_EQ(0, big[99999]);
    memset(big, 0xCD, 100000);
    NAOS_CHECK_TRUE(mnd_yaml_arena_alloc(doc, 8) != NULL);

    NAOS_CHECK_UINT_GE(ta.allocs, 3);
    mnd_yaml_doc_free(doc);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
}

NAOS_TEST(model_node_new_zero_fields)
{
    mnd_yaml_doc_t *doc = NULL;
    mnd_yaml_node_t *scalar = NULL;
    mnd_yaml_node_t *mapping = NULL;
    mnd_yaml_node_t *sequence = NULL;
    mnd_span_t content;

    content.start = 0;
    content.end = 0;
    doc = mnd_yaml_doc_new(NULL, 0, content, MND_EOL_LF, NULL);
    NAOS_REQUIRE_TRUE(doc != NULL);
    scalar = mnd_yaml_node_new(doc, MND_YAML_SCALAR);
    mapping = mnd_yaml_node_new(doc, MND_YAML_MAPPING);
    sequence = mnd_yaml_node_new(doc, MND_YAML_SEQUENCE);
    NAOS_REQUIRE_TRUE(scalar != NULL);
    NAOS_REQUIRE_TRUE(mapping != NULL);
    NAOS_REQUIRE_TRUE(sequence != NULL);

    NAOS_CHECK_INT_EQ((int)MND_YAML_SCALAR, (int)scalar->kind);
    NAOS_CHECK_UINT_EQ(0, scalar->span.start);
    NAOS_CHECK_UINT_EQ(0, scalar->span.end);
    NAOS_CHECK_UINT_EQ(0, scalar->start_line);
    NAOS_CHECK_UINT_EQ(0, scalar->end_line);
    NAOS_CHECK_NULL(scalar->parent);
    NAOS_CHECK_INT_EQ((int)MND_YAML_PLAIN, (int)scalar->as.scalar.style);
    NAOS_CHECK_NULL(scalar->as.scalar.value);
    NAOS_CHECK_UINT_EQ(0, scalar->as.scalar.value_len);
    NAOS_CHECK_FALSE(scalar->as.scalar.flow);
    NAOS_CHECK_INT_EQ((int)MND_YAML_CHOMP_CLIP, (int)scalar->as.scalar.chomp);
    NAOS_CHECK_FALSE(scalar->as.scalar.has_indent_indicator);
    NAOS_CHECK_UINT_EQ(0, scalar->as.scalar.indent_indicator);
    NAOS_CHECK_UINT_EQ(0, scalar->as.scalar.content_indent);

    NAOS_CHECK_INT_EQ((int)MND_YAML_MAPPING, (int)mapping->kind);
    NAOS_CHECK_NULL(mapping->as.mapping.pairs);
    NAOS_CHECK_UINT_EQ(0, mapping->as.mapping.count);
    NAOS_CHECK_INT_EQ((int)MND_YAML_SEQUENCE, (int)sequence->kind);
    NAOS_CHECK_NULL(sequence->as.sequence.items);
    NAOS_CHECK_UINT_EQ(0, sequence->as.sequence.count);
    NAOS_CHECK_TRUE((scalar != mapping) && (mapping != sequence));

    mnd_yaml_doc_set_root(doc, mapping);
    NAOS_CHECK_PTR_EQ(mapping, mnd_yaml_doc_root(doc));
    mnd_yaml_doc_set_root(doc, NULL);
    NAOS_CHECK_NULL(mnd_yaml_doc_root(doc));
    mnd_yaml_doc_free(doc);
}

NAOS_TEST(model_arena_strdup_copies_with_nul)
{
    static const char text[] = "title: Hello";
    mnd_yaml_doc_t *doc = NULL;
    mnd_span_t content;
    char *s = NULL;

    content.start = 0;
    content.end = 0;
    doc = mnd_yaml_doc_new(NULL, 0, content, MND_EOL_LF, NULL);
    NAOS_REQUIRE_TRUE(doc != NULL);
    s = mnd_yaml_arena_strdup(doc, text + 7, 5);
    NAOS_REQUIRE_TRUE(s != NULL);
    NAOS_CHECK_STR_EQ("Hello", s);
    NAOS_CHECK_TRUE(s != (text + 7));
    s = mnd_yaml_arena_strdup(doc, NULL, 0);
    NAOS_REQUIRE_TRUE(s != NULL);
    NAOS_CHECK_STR_EQ("", s);
    s = mnd_yaml_arena_strdup(doc, text, 0);
    NAOS_REQUIRE_TRUE(s != NULL);
    NAOS_CHECK_STR_EQ("", s);
    mnd_yaml_doc_free(doc);
}

/* キーの比較 */

static mnd_yaml_node_t *make_scalar(mnd_yaml_doc_t *doc, mnd_yaml_style_t style, const char *value,
                                    size_t len)
{
    mnd_yaml_node_t *node = mnd_yaml_node_new(doc, MND_YAML_SCALAR);

    if (node == NULL)
    {
        return NULL;
    }
    node->as.scalar.style = style;
    node->as.scalar.value = mnd_yaml_arena_strdup(doc, value, len);
    node->as.scalar.value_len = len;
    if (node->as.scalar.value == NULL)
    {
        return NULL;
    }

    return node;
}

NAOS_TEST(model_key_equal_compares_logical_length_and_content)
{
    mnd_yaml_doc_t *doc = NULL;
    mnd_yaml_node_t *plain = NULL;
    mnd_yaml_node_t *quoted = NULL;
    mnd_yaml_node_t *longer = NULL;
    mnd_yaml_node_t *other = NULL;
    mnd_yaml_node_t *upper = NULL;
    mnd_yaml_node_t *empty1 = NULL;
    mnd_yaml_node_t *empty2 = NULL;
    mnd_yaml_node_t *with_nul = NULL;
    mnd_yaml_node_t *mapping = NULL;
    mnd_span_t content;

    content.start = 0;
    content.end = 0;
    doc = mnd_yaml_doc_new(NULL, 0, content, MND_EOL_LF, NULL);
    NAOS_REQUIRE_TRUE(doc != NULL);
    plain = make_scalar(doc, MND_YAML_PLAIN, "title", 5);
    quoted = make_scalar(doc, MND_YAML_DOUBLE_QUOTED, "title", 5);
    longer = make_scalar(doc, MND_YAML_PLAIN, "titles", 6);
    other = make_scalar(doc, MND_YAML_PLAIN, "tItle", 5);
    upper = make_scalar(doc, MND_YAML_SINGLE_QUOTED, "Title", 5);
    empty1 = make_scalar(doc, MND_YAML_PLAIN, NULL, 0);
    empty2 = make_scalar(doc, MND_YAML_SINGLE_QUOTED, "", 0);
    /* 長さで判定する: NUL 終端の比較なら等しく見える "title\0x" */
    with_nul = make_scalar(doc, MND_YAML_PLAIN, "title\0x", 7);
    mapping = mnd_yaml_node_new(doc, MND_YAML_MAPPING);
    NAOS_REQUIRE_TRUE((plain != NULL) && (quoted != NULL) && (longer != NULL) && (other != NULL));
    NAOS_REQUIRE_TRUE((upper != NULL) && (empty1 != NULL) && (empty2 != NULL) && (with_nul != NULL));
    NAOS_REQUIRE_TRUE(mapping != NULL);

    /* スタイルにかかわらず論理値で一致する */
    NAOS_CHECK_TRUE(mnd_yaml_key_equal(plain, quoted));
    NAOS_CHECK_TRUE(mnd_yaml_key_equal(quoted, plain));
    NAOS_CHECK_TRUE(mnd_yaml_key_equal(plain, plain));
    NAOS_CHECK_TRUE(mnd_yaml_key_equal(empty1, empty2));
    /* 長さが違う、内容が違う、大文字と小文字が違う */
    NAOS_CHECK_FALSE(mnd_yaml_key_equal(plain, longer));
    NAOS_CHECK_FALSE(mnd_yaml_key_equal(longer, plain));
    NAOS_CHECK_FALSE(mnd_yaml_key_equal(plain, other));
    NAOS_CHECK_FALSE(mnd_yaml_key_equal(plain, upper));
    NAOS_CHECK_FALSE(mnd_yaml_key_equal(plain, empty1));
    NAOS_CHECK_FALSE(mnd_yaml_key_equal(plain, with_nul));
    /* スカラーでなければ false */
    NAOS_CHECK_FALSE(mnd_yaml_key_equal(plain, mapping));
    NAOS_CHECK_FALSE(mnd_yaml_key_equal(mapping, mapping));
    mnd_yaml_doc_free(doc);
}

/* 手で組み立てた木と検査関数 */

/*
 * 次の文書（フロントマターの内容 [4, 35) を解析した範囲とする）の木を組み立てる。
 *   0  "---\n"
 *   4  "title: Hello\n"   キー [4, 9)、値 [11, 16)（2 行目）
 *  17  "tags:\n"          キー [17, 21)（3 行目）
 *  23  "  - a\n"          シーケンス [25, 34)、要素 [27, 28)（4 行目）
 *  29  "  - b\n"          要素 [33, 34)（5 行目）
 *  35  "---\n"
 */
static const char sample_text[] = "---\ntitle: Hello\ntags:\n  - a\n  - b\n---\n";

typedef struct sample_tree
{
    mnd_yaml_doc_t *doc;
    mnd_yaml_node_t *root;
    mnd_yaml_node_t *title_key;
    mnd_yaml_node_t *title_value;
    mnd_yaml_node_t *tags_key;
    mnd_yaml_node_t *tags;
    mnd_yaml_node_t *item_a;
    mnd_yaml_node_t *item_b;
} sample_tree_t;

static void set_span(mnd_yaml_node_t *node, size_t start, size_t end, size_t start_line, size_t end_line)
{
    node->span.start = start;
    node->span.end = end;
    node->start_line = start_line;
    node->end_line = end_line;
}

static mnd_yaml_node_t *sample_scalar(mnd_yaml_doc_t *doc, mnd_yaml_node_t *parent, size_t start,
                                      size_t end, size_t line)
{
    mnd_yaml_node_t *node = make_scalar(doc, MND_YAML_PLAIN, sample_text + start, end - start);

    if (node != NULL)
    {
        set_span(node, start, end, line, line);
        node->parent = parent;
    }

    return node;
}

/* 組み立てに失敗したら false（doc は呼び出し側が解放する） */
static bool sample_build(sample_tree_t *t, const mnd_yaml_allocator_t *allocator)
{
    mnd_span_t content;
    mnd_yaml_pair_t *pairs = NULL;
    mnd_yaml_node_t **items = NULL;

    memset(t, 0, sizeof(*t));
    content.start = 4;
    content.end = 35;
    t->doc = mnd_yaml_doc_new(sample_text, sizeof(sample_text) - 1, content, MND_EOL_LF, allocator);
    if (t->doc == NULL)
    {
        return false;
    }
    t->root = mnd_yaml_node_new(t->doc, MND_YAML_MAPPING);
    t->tags = mnd_yaml_node_new(t->doc, MND_YAML_SEQUENCE);
    pairs = (mnd_yaml_pair_t *)mnd_yaml_arena_alloc(t->doc, 2 * sizeof(mnd_yaml_pair_t));
    items = (mnd_yaml_node_t **)mnd_yaml_arena_alloc(t->doc, 2 * sizeof(mnd_yaml_node_t *));
    if ((t->root == NULL) || (t->tags == NULL) || (pairs == NULL) || (items == NULL))
    {
        return false;
    }
    set_span(t->root, 4, 34, 2, 5);
    set_span(t->tags, 25, 34, 4, 5);
    t->tags->parent = t->root;
    t->title_key = sample_scalar(t->doc, t->root, 4, 9, 2);
    t->title_value = sample_scalar(t->doc, t->root, 11, 16, 2);
    t->tags_key = sample_scalar(t->doc, t->root, 17, 21, 3);
    t->item_a = sample_scalar(t->doc, t->tags, 27, 28, 4);
    t->item_b = sample_scalar(t->doc, t->tags, 33, 34, 5);
    if ((t->title_key == NULL) || (t->title_value == NULL) || (t->tags_key == NULL) || (t->item_a == NULL)
        || (t->item_b == NULL))
    {
        return false;
    }
    pairs[0].key = t->title_key;
    pairs[0].value = t->title_value;
    pairs[1].key = t->tags_key;
    pairs[1].value = t->tags;
    t->root->as.mapping.pairs = pairs;
    t->root->as.mapping.count = 2;
    items[0] = t->item_a;
    items[1] = t->item_b;
    t->tags->as.sequence.items = items;
    t->tags->as.sequence.count = 2;
    mnd_yaml_doc_set_root(t->doc, t->root);

    return true;
}

NAOS_TEST(model_tree_check_accepts_hand_built_tree)
{
    mnd_yaml_test_alloc_t ta;
    sample_tree_t t;
    mnd_yaml_tree_check_result_t result;
    bool built;

    mnd_yaml_test_alloc_init(&ta, 0);
    built = sample_build(&t, &ta.allocator);
    NAOS_CHECK_TRUE(built);
    if (built)
    {
        NAOS_CHECK_PTR_EQ(t.root, mnd_yaml_doc_root(t.doc));
        NAOS_CHECK_STR_EQ("Hello", mnd_yaml_doc_root(t.doc)->as.mapping.pairs[0].value->as.scalar.value);
        NAOS_CHECK_TRUE(mnd_yaml_tree_check(t.doc, &result));
        NAOS_CHECK_TRUE(result.ok);
        NAOS_CHECK_NULL(result.node);
        NAOS_CHECK_STR_EQ("", result.message);
        NAOS_CHECK_TRUE(mnd_yaml_tree_check(t.doc, NULL));
        NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(t.doc));
    }
    mnd_yaml_doc_free(t.doc);
    NAOS_CHECK_UINT_EQ(ta.allocs, ta.frees);
}

NAOS_TEST(model_tree_check_accepts_empty_spans_and_crlf_lines)
{
    /* "a:\r\nb: x\r\n" の空の値 [2, 2) と、CRLF の行番号 */
    static const char text[] = "a:\r\nb: x\r\n";
    mnd_yaml_doc_t *doc = NULL;
    mnd_yaml_node_t *root = NULL;
    mnd_yaml_pair_t *pairs = NULL;
    mnd_yaml_node_t *n[4];
    mnd_span_t content;
    size_t i;

    content.start = 0;
    content.end = sizeof(text) - 1;
    doc = mnd_yaml_doc_new(text, sizeof(text) - 1, content, MND_EOL_CRLF, NULL);
    NAOS_REQUIRE_TRUE(doc != NULL);
    root = mnd_yaml_node_new(doc, MND_YAML_MAPPING);
    pairs = (mnd_yaml_pair_t *)mnd_yaml_arena_alloc(doc, 2 * sizeof(mnd_yaml_pair_t));
    NAOS_REQUIRE_TRUE((root != NULL) && (pairs != NULL));
    for (i = 0; i < 4; ++i)
    {
        n[i] = mnd_yaml_node_new(doc, MND_YAML_SCALAR);
        NAOS_REQUIRE_TRUE(n[i] != NULL);
        n[i]->parent = root;
    }
    set_span(root, 0, 8, 1, 2);
    set_span(n[0], 0, 1, 1, 1);
    set_span(n[1], 2, 2, 1, 1);
    set_span(n[2], 4, 5, 2, 2);
    set_span(n[3], 7, 8, 2, 2);
    pairs[0].key = n[0];
    pairs[0].value = n[1];
    pairs[1].key = n[2];
    pairs[1].value = n[3];
    root->as.mapping.pairs = pairs;
    root->as.mapping.count = 2;
    mnd_yaml_doc_set_root(doc, root);
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(doc));

    /* 行末記号の直後（4）は次の行。CR と LF の間（3）は同じ行 */
    set_span(n[1], 2, 4, 1, 2);
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(doc));
    set_span(n[1], 2, 4, 1, 1);
    NAOS_CHECK_FALSE(mnd_yaml_tree_check(doc, NULL));
    set_span(n[1], 2, 3, 1, 1);
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(doc));
    set_span(n[1], 2, 3, 1, 2);
    NAOS_CHECK_FALSE(mnd_yaml_tree_check(doc, NULL));
    mnd_yaml_doc_free(doc);
}

/* 壊した木を検出し、違反したノードを返す */
static void expect_violation(sample_tree_t *t, const mnd_yaml_node_t *node)
{
    mnd_yaml_tree_check_result_t result;

    NAOS_CHECK_FALSE(mnd_yaml_tree_check(t->doc, &result));
    NAOS_CHECK_FALSE(result.ok);
    NAOS_CHECK_PTR_EQ(node, result.node);
    NAOS_CHECK_TRUE(result.message[0] != '\0');
    NAOS_CHECK_FALSE(mnd_yaml_tree_check(t->doc, NULL));
}

NAOS_TEST(model_tree_check_detects_reversed_siblings)
{
    sample_tree_t t;
    mnd_yaml_pair_t tmp;
    mnd_yaml_node_t *item;

    NAOS_REQUIRE_TRUE(sample_build(&t, NULL));
    /* マッピングの項目の逆転 */
    tmp = t.root->as.mapping.pairs[0];
    t.root->as.mapping.pairs[0] = t.root->as.mapping.pairs[1];
    t.root->as.mapping.pairs[1] = tmp;
    expect_violation(&t, t.title_key);
    t.root->as.mapping.pairs[1] = t.root->as.mapping.pairs[0];
    t.root->as.mapping.pairs[0] = tmp;
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(t.doc));

    /* シーケンスの要素の逆転 */
    item = t.tags->as.sequence.items[0];
    t.tags->as.sequence.items[0] = t.tags->as.sequence.items[1];
    t.tags->as.sequence.items[1] = item;
    expect_violation(&t, t.item_a);
    t.tags->as.sequence.items[1] = t.tags->as.sequence.items[0];
    t.tags->as.sequence.items[0] = item;

    /* キーと値の逆転 */
    t.root->as.mapping.pairs[0].key = t.title_value;
    t.root->as.mapping.pairs[0].value = t.title_key;
    expect_violation(&t, t.title_key);
    t.root->as.mapping.pairs[0].key = t.title_key;
    t.root->as.mapping.pairs[0].value = t.title_value;

    /* 重なり */
    set_span(t.item_a, 27, 34, 4, 5);
    expect_violation(&t, t.item_b);
    set_span(t.item_a, 27, 28, 4, 4);
    NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(t.doc));
    mnd_yaml_doc_free(t.doc);
}

NAOS_TEST(model_tree_check_detects_child_outside_parent)
{
    sample_tree_t t;
    mnd_yaml_node_t *wrong;

    NAOS_REQUIRE_TRUE(sample_build(&t, NULL));
    /* 親の範囲の外の子 */
    set_span(t.tags, 25, 30, 4, 5);
    expect_violation(&t, t.item_b);
    set_span(t.tags, 25, 34, 4, 5);

    /* 子の parent が親を指さない */
    t.item_a->parent = t.root;
    expect_violation(&t, t.item_a);
    t.item_a->parent = t.tags;

    /* 根の parent が NULL でない */
    t.root->parent = t.tags;
    expect_violation(&t, t.root);
    t.root->parent = NULL;

    /* 内容の範囲の外 */
    set_span(t.root, 0, 34, 1, 5);
    expect_violation(&t, t.root);
    set_span(t.root, 4, 36, 2, 6);
    expect_violation(&t, t.root);
    set_span(t.root, 4, 34, 2, 5);

    /* 開始 > 終了 */
    set_span(t.item_a, 28, 27, 4, 4);
    expect_violation(&t, t.item_a);
    set_span(t.item_a, 27, 28, 4, 4);

    /* 行番号の誤り */
    t.title_value->start_line = 1;
    expect_violation(&t, t.title_value);
    t.title_value->start_line = 2;
    t.tags->end_line = 4;
    expect_violation(&t, t.tags);
    t.tags->end_line = 5;

    /* 欠けた子（親のノードを返す）とスカラーでないキー（キーのノードを返す） */
    wrong = t.root->as.mapping.pairs[1].value;
    t.root->as.mapping.pairs[1].value = NULL;
    expect_violation(&t, t.root);
    t.root->as.mapping.pairs[1].value = wrong;
    t.tags->as.sequence.items[1] = NULL;
    expect_violation(&t, t.tags);
    t.tags->as.sequence.items[1] = t.item_b;
    t.tags->kind = MND_YAML_MAPPING;
    t.tags->as.mapping.pairs = NULL;
    t.tags->as.mapping.count = 0;
    t.root->as.mapping.pairs[0].key = t.tags;
    t.root->as.mapping.pairs[0].value = t.tags_key;
    t.root->as.mapping.pairs[1].key = t.title_key;
    expect_violation(&t, t.tags);

    NAOS_CHECK_FALSE(mnd_yaml_tree_check(NULL, NULL));
    NAOS_CHECK_FALSE(mnd_yaml_tree_check_note(NULL));
    mnd_yaml_doc_free(t.doc);
}

NAOS_SUITE(model,
    NAOS_ADD_TEST(test_alloc_counts_allocs_and_frees),
    NAOS_ADD_TEST(test_alloc_fails_only_at_nth_alloc),
    NAOS_ADD_TEST(model_scaffold_default_allocator_and_size_mul),
    NAOS_ADD_TEST(model_doc_new_empty_and_accessors),
    NAOS_ADD_TEST(model_doc_new_keeps_crlf_and_accepts_null_data),
    NAOS_ADD_TEST(model_doc_new_returns_null_on_alloc_failure),
    NAOS_ADD_TEST(model_arena_returns_null_on_alloc_failure),
    NAOS_ADD_TEST(model_arena_alloc_is_zeroed_aligned_and_freed_at_once),
    NAOS_ADD_TEST(model_node_new_zero_fields),
    NAOS_ADD_TEST(model_arena_strdup_copies_with_nul),
    NAOS_ADD_TEST(model_key_equal_compares_logical_length_and_content),
    NAOS_ADD_TEST(model_tree_check_accepts_hand_built_tree),
    NAOS_ADD_TEST(model_tree_check_accepts_empty_spans_and_crlf_lines),
    NAOS_ADD_TEST(model_tree_check_detects_reversed_siblings),
    NAOS_ADD_TEST(model_tree_check_detects_child_outside_parent)
)
