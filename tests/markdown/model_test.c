/* Model: 構文木の型、アリーナ、走査、解放と、テスト用の確保関数 */

#include <naos/naos.h>

#include "markdown/md.h"
#include "markdown/md_internal.h"
#include "test_alloc.h"

#include <stdint.h>
#include <string.h>

/* 深い木のテストの深さ（8.3、8.5） */
#define DEEP_TREE_DEPTH 100000

static mnd_span_t span_of(size_t start, size_t end)
{
    mnd_span_t span;

    span.start = start;
    span.end = end;

    return span;
}

/* 確保したものがすべて解放されたことを確かめる */
static void check_balanced(const mnd_md_test_alloc_t *ta)
{
    NAOS_CHECK_UINT_EQ(ta->allocs, ta->frees);
    NAOS_CHECK_UINT_EQ(0, mnd_md_test_alloc_live(ta));
}

/* ptr がどの型にも使える境界（ここでは long double、void *、long long の境界）に揃っているか */
static bool is_max_aligned(const void *ptr)
{
    uintptr_t p = (uintptr_t)ptr;

    return ((p % sizeof(long double)) == 0) && ((p % sizeof(void *)) == 0)
           && ((p % sizeof(long long)) == 0);
}

static bool is_zero(const void *ptr, size_t size)
{
    const unsigned char *p = (const unsigned char *)ptr;
    size_t i = 0;

    for (i = 0; i < size; i++)
    {
        if (p[i] != 0)
        {
            return false;
        }
    }

    return true;
}

NAOS_TEST(model_kind_dialect_starts_after_commonmark_kinds)
{
    NAOS_CHECK_INT_EQ(MND_MD_KIND_DIALECT, 0x100);
    NAOS_CHECK_INT_GT(MND_MD_KIND_DIALECT, MND_MD_HTML_INLINE);
}

/* test_alloc */

NAOS_TEST(test_alloc_counts_allocs_and_frees)
{
    mnd_md_test_alloc_t ta;
    void *a = NULL;
    void *b = NULL;

    mnd_md_test_alloc_init(&ta, 0);
    a = ta.allocator.alloc(ta.allocator.ctx, 16);
    b = ta.allocator.alloc(ta.allocator.ctx, 0);
    NAOS_REQUIRE_NOT_NULL(a);
    NAOS_REQUIRE_NOT_NULL(b);
    NAOS_CHECK_UINT_EQ(2, ta.calls);
    NAOS_CHECK_UINT_EQ(2, ta.allocs);
    NAOS_CHECK_UINT_EQ(2, mnd_md_test_alloc_live(&ta));
    ta.allocator.free(ta.allocator.ctx, a);
    ta.allocator.free(ta.allocator.ctx, NULL); /* NULL は数えない */
    NAOS_CHECK_UINT_EQ(1, ta.frees);
    ta.allocator.free(ta.allocator.ctx, b);
    NAOS_CHECK_UINT_EQ(0, ta.failures);
    check_balanced(&ta);
}

NAOS_TEST(test_alloc_fails_only_at_nth_alloc)
{
    mnd_md_test_alloc_t ta;
    void *p[4] = { NULL, NULL, NULL, NULL };
    size_t i = 0;

    mnd_md_test_alloc_init(&ta, 3);
    for (i = 0; i < 4; i++)
    {
        p[i] = ta.allocator.alloc(ta.allocator.ctx, 8);
    }
    NAOS_CHECK_NOT_NULL(p[0]);
    NAOS_CHECK_NOT_NULL(p[1]);
    NAOS_CHECK_NULL(p[2]);
    NAOS_CHECK_NOT_NULL(p[3]);
    NAOS_CHECK_UINT_EQ(4, ta.calls);
    NAOS_CHECK_UINT_EQ(3, ta.allocs);
    NAOS_CHECK_UINT_EQ(1, ta.failures);
    for (i = 0; i < 4; i++)
    {
        ta.allocator.free(ta.allocator.ctx, p[i]);
    }
    check_balanced(&ta);
}

/* 確保関数の既定値と容量の計算 */

NAOS_TEST(null_allocator_defaults_to_malloc_and_free)
{
    const mnd_md_allocator_t *a = mnd_md_allocator_or_default(NULL);
    mnd_md_test_alloc_t ta;
    void *p = NULL;

    NAOS_REQUIRE_NOT_NULL(a);
    NAOS_REQUIRE_TRUE(a->alloc != NULL);
    NAOS_REQUIRE_TRUE(a->free != NULL);
    p = a->alloc(a->ctx, 32);
    NAOS_REQUIRE_NOT_NULL(p);
    memset(p, 0xAB, 32);
    a->free(a->ctx, p);
    a->free(a->ctx, NULL);

    mnd_md_test_alloc_init(&ta, 0);
    NAOS_CHECK_PTR_EQ(&ta.allocator, mnd_md_allocator_or_default(&ta.allocator));
}

NAOS_TEST(size_mul_detects_overflow)
{
    size_t out = 7;

    NAOS_CHECK_TRUE(mnd_md_size_mul(3, 5, &out));
    NAOS_CHECK_UINT_EQ(15, out);
    NAOS_CHECK_TRUE(mnd_md_size_mul(0, SIZE_MAX, &out));
    NAOS_CHECK_UINT_EQ(0, out);
    NAOS_CHECK_TRUE(mnd_md_size_mul(SIZE_MAX, 1, &out));
    NAOS_CHECK_UINT_EQ(SIZE_MAX, out);
    out = 7;
    NAOS_CHECK_FALSE(mnd_md_size_mul((SIZE_MAX / 2) + 1, 2, &out));
    NAOS_CHECK_FALSE(mnd_md_size_mul(SIZE_MAX, SIZE_MAX, &out));
    NAOS_CHECK_UINT_EQ(7, out);
}

/* アリーナ */

NAOS_TEST(arena_allocations_are_aligned_and_zeroed)
{
    mnd_md_test_alloc_t ta;
    mnd_md_arena_t arena;
    size_t sizes[] = { 1, 3, 7, 8, 13, 24, 0, 100, 5 };
    void *ptrs[sizeof(sizes) / sizeof(sizes[0])];
    size_t i = 0;

    mnd_md_test_alloc_init(&ta, 0);
    mnd_md_arena_init(&arena, &ta.allocator);
    NAOS_CHECK_UINT_EQ(0, ta.calls); /* 初期化では確保しない */
    for (i = 0; i < (sizeof(sizes) / sizeof(sizes[0])); i++)
    {
        ptrs[i] = mnd_md_arena_alloc(&arena, sizes[i]);
        NAOS_REQUIRE_NOT_NULL(ptrs[i]);
        NAOS_CHECK_TRUE(is_max_aligned(ptrs[i]));
        NAOS_CHECK_TRUE(is_zero(ptrs[i], sizes[i]));
        memset(ptrs[i], 0xFF, sizes[i]);
    }
    /* 切り出した領域は互いに重ならない（書き込みが別の領域を壊さない） */
    for (i = 0; i < (sizeof(sizes) / sizeof(sizes[0])); i++)
    {
        size_t k = 0;

        for (k = i + 1; k < (sizeof(sizes) / sizeof(sizes[0])); k++)
        {
            NAOS_CHECK_TRUE(ptrs[i] != ptrs[k]);
        }
    }
    /* 小さい確保は 1 つの塊にまとまる */
    NAOS_CHECK_UINT_EQ(1, ta.allocs);
    mnd_md_arena_free(&arena);
    check_balanced(&ta);
}

NAOS_TEST(arena_big_and_many_allocations)
{
    mnd_md_test_alloc_t ta;
    mnd_md_arena_t arena;
    unsigned char *big = NULL;
    unsigned char *small = NULL;
    size_t i = 0;

    mnd_md_test_alloc_init(&ta, 0);
    mnd_md_arena_init(&arena, &ta.allocator);
    small = (unsigned char *)mnd_md_arena_alloc(&arena, 10);
    NAOS_REQUIRE_NOT_NULL(small);
    big = (unsigned char *)mnd_md_arena_alloc(&arena, 1000000);
    NAOS_REQUIRE_NOT_NULL(big);
    NAOS_CHECK_TRUE(is_max_aligned(big));
    NAOS_CHECK_TRUE(is_zero(big, 1000000));
    memset(big, 0x5A, 1000000);
    NAOS_CHECK_TRUE(is_zero(small, 10));
    for (i = 0; i < 10000; i++)
    {
        unsigned char *p = (unsigned char *)mnd_md_arena_alloc(&arena, 48);

        NAOS_REQUIRE_NOT_NULL(p);
        NAOS_REQUIRE_TRUE(is_zero(p, 48));
        memset(p, 0x11, 48);
    }
    NAOS_CHECK_UINT_EQ(0x5A, big[999999]);
    NAOS_CHECK_UINT_GT(ta.allocs, 2);
    mnd_md_arena_free(&arena);
    check_balanced(&ta);
    /* 解放したあとも続けて使える */
    NAOS_REQUIRE_NOT_NULL(mnd_md_arena_alloc(&arena, 8));
    mnd_md_arena_free(&arena);
    mnd_md_arena_free(&arena);
    check_balanced(&ta);
}

NAOS_TEST(arena_dup_and_array)
{
    mnd_md_test_alloc_t ta;
    mnd_md_arena_t arena;
    const char text[] = "abcdef";
    char *copy = NULL;
    long *values = NULL;

    mnd_md_test_alloc_init(&ta, 0);
    mnd_md_arena_init(&arena, &ta.allocator);
    copy = (char *)mnd_md_arena_dup(&arena, text, sizeof(text));
    NAOS_REQUIRE_NOT_NULL(copy);
    NAOS_CHECK_TRUE(copy != text);
    NAOS_CHECK_STR_EQ("abcdef", copy);
    NAOS_CHECK_NOT_NULL(mnd_md_arena_dup(&arena, NULL, 0));
    values = (long *)mnd_md_arena_alloc_array(&arena, 100, sizeof(long));
    NAOS_REQUIRE_NOT_NULL(values);
    NAOS_CHECK_TRUE(is_zero(values, 100 * sizeof(long)));
    mnd_md_arena_free(&arena);
    check_balanced(&ta);
}

NAOS_TEST(arena_refuses_overflowing_sizes_before_allocating)
{
    mnd_md_test_alloc_t ta;
    mnd_md_arena_t arena;

    mnd_md_test_alloc_init(&ta, 0);
    mnd_md_arena_init(&arena, &ta.allocator);
    NAOS_CHECK_NULL(mnd_md_arena_alloc(&arena, SIZE_MAX));
    NAOS_CHECK_NULL(mnd_md_arena_alloc(&arena, SIZE_MAX - 3));
    NAOS_CHECK_NULL(mnd_md_arena_alloc_array(&arena, (SIZE_MAX / 2) + 1, 2));
    NAOS_CHECK_NULL(mnd_md_arena_alloc_array(&arena, SIZE_MAX, SIZE_MAX));
    NAOS_CHECK_UINT_EQ(0, ta.calls);
    mnd_md_arena_free(&arena);
    check_balanced(&ta);
}

NAOS_TEST(arena_reports_allocation_failure)
{
    mnd_md_test_alloc_t ta;
    mnd_md_arena_t arena;

    mnd_md_test_alloc_init(&ta, 1);
    mnd_md_arena_init(&arena, &ta.allocator);
    NAOS_CHECK_NULL(mnd_md_arena_alloc(&arena, 16));
    NAOS_CHECK_UINT_EQ(1, ta.failures);
    /* 失敗のあとも使える */
    NAOS_CHECK_NOT_NULL(mnd_md_arena_alloc(&arena, 16));
    mnd_md_arena_free(&arena);
    check_balanced(&ta);

    /* 大きい確保（専用の塊）の失敗 */
    mnd_md_test_alloc_init(&ta, 2);
    mnd_md_arena_init(&arena, &ta.allocator);
    NAOS_REQUIRE_NOT_NULL(mnd_md_arena_alloc(&arena, 16));
    NAOS_CHECK_NULL(mnd_md_arena_alloc(&arena, 1000000));
    NAOS_CHECK_UINT_EQ(1, ta.failures);
    mnd_md_arena_free(&arena);
    check_balanced(&ta);
}

/* 作業用の配列 */

NAOS_TEST(work_array_grows_and_keeps_contents)
{
    mnd_md_test_alloc_t ta;
    mnd_md_array_t array;
    size_t i = 0;

    mnd_md_test_alloc_init(&ta, 0);
    mnd_md_array_init(&array, sizeof(size_t), &ta.allocator);
    NAOS_CHECK_UINT_EQ(0, array.count);
    NAOS_CHECK_NULL(mnd_md_array_last(&array));
    NAOS_CHECK_NULL(mnd_md_array_at(&array, 0));
    NAOS_CHECK_UINT_EQ(0, ta.calls);
    for (i = 0; i < 1000; i++)
    {
        size_t *slot = (size_t *)mnd_md_array_push(&array);

        NAOS_REQUIRE_NOT_NULL(slot);
        NAOS_REQUIRE_UINT_EQ(0, *slot);
        *slot = i * 3;
    }
    NAOS_CHECK_UINT_EQ(1000, array.count);
    NAOS_CHECK_UINT_GE(array.cap, 1000);
    /* 容量を倍に伸ばすので、確保の回数は要素数の対数程度 */
    NAOS_CHECK_UINT_LE(ta.allocs, 20);
    for (i = 0; i < 1000; i++)
    {
        size_t *slot = (size_t *)mnd_md_array_at(&array, i);

        NAOS_REQUIRE_NOT_NULL(slot);
        NAOS_REQUIRE_UINT_EQ(i * 3, *slot);
    }
    NAOS_CHECK_NULL(mnd_md_array_at(&array, 1000));
    NAOS_CHECK_UINT_EQ(999 * 3, *(size_t *)mnd_md_array_last(&array));
    mnd_md_array_pop(&array);
    NAOS_CHECK_UINT_EQ(999, array.count);
    NAOS_CHECK_UINT_EQ(998 * 3, *(size_t *)mnd_md_array_last(&array));
    i = 42;
    NAOS_REQUIRE_TRUE(mnd_md_array_append(&array, &i));
    NAOS_CHECK_UINT_EQ(42, *(size_t *)mnd_md_array_last(&array));
    mnd_md_array_clear(&array);
    NAOS_CHECK_UINT_EQ(0, array.count);
    mnd_md_array_pop(&array);
    NAOS_CHECK_UINT_EQ(0, array.count);
    mnd_md_array_free(&array);
    NAOS_CHECK_NULL(array.items);
    NAOS_CHECK_UINT_EQ(0, array.cap);
    check_balanced(&ta);
    /* 解放したあとも続けて使える */
    NAOS_REQUIRE_NOT_NULL(mnd_md_array_push(&array));
    mnd_md_array_free(&array);
    mnd_md_array_free(&array);
    check_balanced(&ta);
}

NAOS_TEST(work_array_reports_allocation_failure)
{
    mnd_md_test_alloc_t ta;
    mnd_md_array_t array;
    size_t i = 0;
    size_t pushed = 0;

    /* 2 回目の確保（最初の伸長）で失敗させる */
    mnd_md_test_alloc_init(&ta, 2);
    mnd_md_array_init(&array, sizeof(size_t), &ta.allocator);
    for (i = 0; i < 1000; i++)
    {
        size_t *slot = (size_t *)mnd_md_array_push(&array);

        if (slot == NULL)
        {
            break;
        }
        *slot = i;
        pushed++;
    }
    NAOS_REQUIRE_UINT_LT(pushed, 1000);
    NAOS_CHECK_UINT_EQ(1, ta.failures);
    /* 失敗しても、配列はそれまでの内容のまま */
    NAOS_CHECK_UINT_EQ(pushed, array.count);
    for (i = 0; i < pushed; i++)
    {
        NAOS_CHECK_UINT_EQ(i, *(size_t *)mnd_md_array_at(&array, i));
    }
    /* 次の確保は成功する */
    NAOS_CHECK_NOT_NULL(mnd_md_array_push(&array));
    NAOS_CHECK_UINT_EQ(pushed + 1, array.count);
    mnd_md_array_free(&array);
    check_balanced(&ta);
}

NAOS_TEST(work_array_refuses_overflowing_capacity)
{
    mnd_md_test_alloc_t ta;
    mnd_md_array_t array;

    mnd_md_test_alloc_init(&ta, 0);
    mnd_md_array_init(&array, sizeof(size_t), &ta.allocator);
    NAOS_CHECK_FALSE(mnd_md_array_reserve(&array, SIZE_MAX));
    NAOS_CHECK_FALSE(mnd_md_array_reserve(&array, (SIZE_MAX / sizeof(size_t)) + 1));
    NAOS_CHECK_UINT_EQ(0, ta.calls);
    NAOS_REQUIRE_NOT_NULL(mnd_md_array_push(&array));
    /* count + additional のオーバーフロー */
    NAOS_CHECK_FALSE(mnd_md_array_reserve(&array, SIZE_MAX));
    NAOS_CHECK_UINT_EQ(1, ta.calls);
    NAOS_CHECK_UINT_EQ(1, array.count);
    mnd_md_array_free(&array);

    /* 要素が大きく、容量 * 要素の大きさがオーバーフローする */
    mnd_md_array_init(&array, (SIZE_MAX / 2) + 1, &ta.allocator);
    NAOS_CHECK_FALSE(mnd_md_array_reserve(&array, 2));
    NAOS_CHECK_NULL(mnd_md_array_push(&array));
    NAOS_CHECK_UINT_EQ(0, array.count);
    NAOS_CHECK_UINT_EQ(1, ta.calls);
    mnd_md_array_free(&array);
    check_balanced(&ta);
}

NAOS_TEST(work_array_reserve_zero_and_default_allocator)
{
    mnd_md_array_t array;
    int value = 7;

    mnd_md_array_init(&array, sizeof(int), NULL);
    NAOS_CHECK_TRUE(mnd_md_array_reserve(&array, 0));
    NAOS_REQUIRE_TRUE(mnd_md_array_reserve(&array, 10));
    NAOS_CHECK_UINT_GE(array.cap, 10);
    NAOS_CHECK_UINT_EQ(0, array.count);
    NAOS_REQUIRE_TRUE(mnd_md_array_append(&array, &value));
    NAOS_CHECK_INT_EQ(7, *(int *)mnd_md_array_at(&array, 0));
    mnd_md_array_free(&array);
}

/* 文書 */

NAOS_TEST(doc_create_and_accessors)
{
    mnd_md_test_alloc_t ta;
    const char text[] = "# hi\n";
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *root = NULL;

    mnd_md_test_alloc_init(&ta, 0);
    doc = mnd_md_doc_create(text, 5, &ta.allocator);
    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_PTR_EQ(text, mnd_md_doc_data(doc));
    NAOS_CHECK_UINT_EQ(5, mnd_md_doc_len(doc));
    root = mnd_md_doc_root(doc);
    NAOS_REQUIRE_NOT_NULL(root);
    NAOS_CHECK_INT_EQ(MND_MD_DOCUMENT, root->kind);
    NAOS_CHECK_UINT_EQ(0, root->span.start);
    NAOS_CHECK_UINT_EQ(5, root->span.end);
    NAOS_CHECK_NULL(root->parent);
    NAOS_CHECK_NULL(root->first_child);
    NAOS_CHECK_NULL(root->last_child);
    NAOS_CHECK_NULL(root->prev);
    NAOS_CHECK_NULL(root->next);
    NAOS_CHECK_NULL(root->lines);
    NAOS_CHECK_UINT_EQ(0, root->line_count);
    NAOS_CHECK_NULL(mnd_md_node_next(root, root));
    mnd_md_doc_free(doc);
    check_balanced(&ta);
}

NAOS_TEST(doc_empty_content_and_default_allocator)
{
    mnd_md_doc_t *doc = mnd_md_doc_create(NULL, 0, NULL);

    NAOS_REQUIRE_NOT_NULL(doc);
    NAOS_CHECK_NULL(mnd_md_doc_data(doc));
    NAOS_CHECK_UINT_EQ(0, mnd_md_doc_len(doc));
    NAOS_REQUIRE_NOT_NULL(mnd_md_doc_root(doc));
    NAOS_CHECK_UINT_EQ(0, mnd_md_doc_root(doc)->span.end);
    mnd_md_doc_free(doc);
}

NAOS_TEST(doc_free_and_accessors_accept_null)
{
    mnd_md_doc_free(NULL);
    NAOS_CHECK_NULL(mnd_md_doc_root(NULL));
    NAOS_CHECK_NULL(mnd_md_doc_data(NULL));
    NAOS_CHECK_UINT_EQ(0, mnd_md_doc_len(NULL));
    NAOS_CHECK_NULL(mnd_md_node_next(NULL, NULL));
}

NAOS_TEST(doc_create_reports_allocation_failure)
{
    size_t n = 0;
    bool succeeded = false;

    /* n 回目の確保を失敗させ、成功するまで n を増やす */
    for (n = 1; (n <= 100) && !succeeded; n++)
    {
        mnd_md_test_alloc_t ta;
        mnd_md_doc_t *doc = NULL;

        mnd_md_test_alloc_init(&ta, n);
        doc = mnd_md_doc_create("abc", 3, &ta.allocator);
        if (ta.failures == 0)
        {
            NAOS_REQUIRE_NOT_NULL(doc);
            succeeded = true;
        }
        else
        {
            NAOS_CHECK_NULL(doc);
        }
        mnd_md_doc_free(doc);
        check_balanced(&ta);
    }
    NAOS_CHECK_TRUE(succeeded);
    /* 少なくとも 1 回は失敗を返した */
    NAOS_CHECK_UINT_GT(n, 2);
}

/* ノードの作成と連結 */

NAOS_TEST(node_new_sets_kind_and_span)
{
    mnd_md_doc_t *doc = mnd_md_doc_create("abcdef", 6, NULL);
    mnd_md_node_t *node = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    node = mnd_md_node_new(doc, MND_MD_PARAGRAPH, span_of(1, 4));
    NAOS_REQUIRE_NOT_NULL(node);
    NAOS_CHECK_INT_EQ(MND_MD_PARAGRAPH, node->kind);
    NAOS_CHECK_UINT_EQ(1, node->span.start);
    NAOS_CHECK_UINT_EQ(4, node->span.end);
    NAOS_CHECK_NULL(node->parent);
    NAOS_CHECK_NULL(node->first_child);
    NAOS_CHECK_NULL(node->last_child);
    NAOS_CHECK_NULL(node->prev);
    NAOS_CHECK_NULL(node->next);
    NAOS_CHECK_NULL(node->lines);
    NAOS_CHECK_UINT_EQ(0, node->line_count);
    NAOS_CHECK_UINT_EQ(0, node->start_line);
    NAOS_CHECK_UINT_EQ(0, node->end_line);
    NAOS_CHECK_TRUE(is_zero(&node->as, sizeof(node->as)));
    NAOS_CHECK_TRUE(is_max_aligned(node));
    mnd_md_doc_free(doc);
}

NAOS_TEST(node_new_reports_allocation_failure)
{
    mnd_md_test_alloc_t ta;
    mnd_md_doc_t *doc = NULL;
    mnd_md_node_t *node = NULL;
    size_t i = 0;
    size_t created = 0;

    mnd_md_test_alloc_init(&ta, 0);
    doc = mnd_md_doc_create("x", 1, &ta.allocator);
    NAOS_REQUIRE_NOT_NULL(doc);
    /* 次の確保（アリーナの次の塊）で失敗させる */
    ta.fail_at = ta.calls + 1;
    for (i = 0; i < 1000000; i++)
    {
        node = mnd_md_node_new(doc, MND_MD_TEXT, span_of(0, 1));
        if (node == NULL)
        {
            break;
        }
        mnd_md_node_append_child(doc->root, node);
        created++;
    }
    NAOS_CHECK_NULL(node);
    NAOS_CHECK_UINT_EQ(1, ta.failures);
    NAOS_CHECK_UINT_LT(created, 1000000);
    /* 失敗のあとも作れる */
    node = mnd_md_node_new(doc, MND_MD_TEXT, span_of(0, 1));
    NAOS_REQUIRE_NOT_NULL(node);
    mnd_md_node_append_child(doc->root, node);
    mnd_md_doc_free(doc);
    check_balanced(&ta);
}

/* parent の子が nodes[0..count) の順に並び、連結が正しいことを確かめる */
static void check_children(const mnd_md_node_t *parent, mnd_md_node_t *const *nodes, size_t count)
{
    const mnd_md_node_t *child = parent->first_child;
    const mnd_md_node_t *prev = NULL;
    size_t i = 0;

    for (i = 0; i < count; i++)
    {
        NAOS_REQUIRE_TRUE(child == nodes[i]);
        NAOS_CHECK_TRUE(child->parent == parent);
        NAOS_CHECK_TRUE(child->prev == prev);
        prev = child;
        child = child->next;
    }
    NAOS_CHECK_NULL(child);
    NAOS_CHECK_TRUE(parent->last_child == prev);
}

/* parent の子が、引数の NULL でないノードの順に並び、連結が正しいことを確かめる */
static void check_kids(const mnd_md_node_t *parent, mnd_md_node_t *k0, mnd_md_node_t *k1,
                       mnd_md_node_t *k2, mnd_md_node_t *k3, mnd_md_node_t *k4, mnd_md_node_t *k5)
{
    mnd_md_node_t *nodes[6];
    size_t count = 0;

    nodes[0] = k0;
    nodes[1] = k1;
    nodes[2] = k2;
    nodes[3] = k3;
    nodes[4] = k4;
    nodes[5] = k5;
    while ((count < 6) && (nodes[count] != NULL))
    {
        count++;
    }
    check_children(parent, nodes, count);
}

static bool is_detached(const mnd_md_node_t *node)
{
    return (node->parent == NULL) && (node->prev == NULL) && (node->next == NULL);
}

NAOS_TEST(append_child_keeps_order)
{
    mnd_md_doc_t *doc = mnd_md_doc_create("abc", 3, NULL);
    mnd_md_node_t *n[3];
    size_t i = 0;

    NAOS_REQUIRE_NOT_NULL(doc);
    for (i = 0; i < 3; i++)
    {
        n[i] = mnd_md_node_new(doc, MND_MD_PARAGRAPH, span_of(i, i + 1));
        NAOS_REQUIRE_NOT_NULL(n[i]);
        mnd_md_node_append_child(doc->root, n[i]);
        check_children(doc->root, n, i + 1);
    }
    mnd_md_doc_free(doc);
}

NAOS_TEST(insert_before_and_after_keep_links)
{
    mnd_md_doc_t *doc = mnd_md_doc_create("abcdef", 6, NULL);
    mnd_md_node_t *a = NULL;
    mnd_md_node_t *b = NULL;
    mnd_md_node_t *c = NULL;
    mnd_md_node_t *d = NULL;
    mnd_md_node_t *e = NULL;
    mnd_md_node_t *f = NULL;

    NAOS_REQUIRE_NOT_NULL(doc);
    a = mnd_md_node_new(doc, MND_MD_TEXT, span_of(0, 1));
    b = mnd_md_node_new(doc, MND_MD_TEXT, span_of(1, 2));
    c = mnd_md_node_new(doc, MND_MD_TEXT, span_of(2, 3));
    d = mnd_md_node_new(doc, MND_MD_TEXT, span_of(3, 4));
    e = mnd_md_node_new(doc, MND_MD_TEXT, span_of(4, 5));
    f = mnd_md_node_new(doc, MND_MD_TEXT, span_of(5, 6));
    NAOS_REQUIRE_TRUE((a != NULL) && (b != NULL) && (c != NULL) && (d != NULL) && (e != NULL)
                      && (f != NULL));

    mnd_md_node_append_child(doc->root, c);
    /* 唯一の子の前 → 最初の子が変わる */
    mnd_md_node_insert_before(c, a);
    check_kids(doc->root, a, c, NULL, NULL, NULL, NULL);
    /* 中ほどの前 */
    mnd_md_node_insert_before(c, b);
    check_kids(doc->root, a, b, c, NULL, NULL, NULL);
    /* 最後の子の後ろ → 最後の子が変わる */
    mnd_md_node_insert_after(c, e);
    check_kids(doc->root, a, b, c, e, NULL, NULL);
    /* 中ほどの後ろ */
    mnd_md_node_insert_after(c, d);
    check_kids(doc->root, a, b, c, d, e, NULL);
    /* 連結しているノードを移すと、元の位置から外れる */
    mnd_md_node_insert_before(a, e);
    check_kids(doc->root, e, a, b, c, d, NULL);
    mnd_md_node_append_child(d, f);
    mnd_md_node_append_child(doc->root, f);
    check_kids(doc->root, e, a, b, c, d, f);
    check_children(d, NULL, 0);
    mnd_md_doc_free(doc);
}

NAOS_TEST(unlink_removes_first_middle_last_and_only)
{
    mnd_md_doc_t *doc = mnd_md_doc_create("abcd", 4, NULL);
    mnd_md_node_t *n[4];
    size_t i = 0;

    NAOS_REQUIRE_NOT_NULL(doc);
    for (i = 0; i < 4; i++)
    {
        n[i] = mnd_md_node_new(doc, MND_MD_PARAGRAPH, span_of(i, i + 1));
        NAOS_REQUIRE_NOT_NULL(n[i]);
        mnd_md_node_append_child(doc->root, n[i]);
    }
    /* 中ほど */
    mnd_md_node_unlink(n[1]);
    NAOS_CHECK_TRUE(is_detached(n[1]));
    check_kids(doc->root, n[0], n[2], n[3], NULL, NULL, NULL);
    /* 最初 */
    mnd_md_node_unlink(n[0]);
    NAOS_CHECK_TRUE(is_detached(n[0]));
    check_kids(doc->root, n[2], n[3], NULL, NULL, NULL, NULL);
    /* 最後 */
    mnd_md_node_unlink(n[3]);
    NAOS_CHECK_TRUE(is_detached(n[3]));
    check_kids(doc->root, n[2], NULL, NULL, NULL, NULL, NULL);
    /* 唯一の子 */
    mnd_md_node_unlink(n[2]);
    NAOS_CHECK_TRUE(is_detached(n[2]));
    check_children(doc->root, n, 0);
    /* 連結していないノードでは何もしない */
    mnd_md_node_unlink(n[2]);
    NAOS_CHECK_TRUE(is_detached(n[2]));
    /* 子を持ったまま外して、別の場所へ移せる */
    mnd_md_node_append_child(n[0], n[1]);
    mnd_md_node_append_child(doc->root, n[0]);
    mnd_md_node_unlink(n[0]);
    NAOS_CHECK_TRUE(n[0]->first_child == n[1]);
    NAOS_CHECK_TRUE(n[1]->parent == n[0]);
    mnd_md_doc_free(doc);
}

/* 走査 */

NAOS_TEST(node_next_is_preorder_and_bounded_to_subtree)
{
    /*
     * root
     * ├─ a
     * │  ├─ a1
     * │  │  └─ a1x
     * │  └─ a2
     * ├─ b
     * └─ c
     *    └─ c1
     */
    mnd_md_doc_t *doc = mnd_md_doc_create("0123456789", 10, NULL);
    mnd_md_node_t *root = NULL;
    mnd_md_node_t *a = NULL;
    mnd_md_node_t *a1 = NULL;
    mnd_md_node_t *a1x = NULL;
    mnd_md_node_t *a2 = NULL;
    mnd_md_node_t *b = NULL;
    mnd_md_node_t *c = NULL;
    mnd_md_node_t *c1 = NULL;
    const mnd_md_node_t *expect[8];
    const mnd_md_node_t *it = NULL;
    size_t i = 0;

    NAOS_REQUIRE_NOT_NULL(doc);
    root = doc->root;
    a = mnd_md_node_new(doc, MND_MD_BLOCK_QUOTE, span_of(0, 4));
    a1 = mnd_md_node_new(doc, MND_MD_LIST, span_of(0, 2));
    a1x = mnd_md_node_new(doc, MND_MD_ITEM, span_of(0, 1));
    a2 = mnd_md_node_new(doc, MND_MD_PARAGRAPH, span_of(3, 4));
    b = mnd_md_node_new(doc, MND_MD_THEMATIC_BREAK, span_of(5, 6));
    c = mnd_md_node_new(doc, MND_MD_PARAGRAPH, span_of(7, 9));
    c1 = mnd_md_node_new(doc, MND_MD_TEXT, span_of(7, 9));
    NAOS_REQUIRE_TRUE((a != NULL) && (a1 != NULL) && (a1x != NULL) && (a2 != NULL) && (b != NULL)
                      && (c != NULL) && (c1 != NULL));
    mnd_md_node_append_child(root, a);
    mnd_md_node_append_child(a, a1);
    mnd_md_node_append_child(a1, a1x);
    mnd_md_node_append_child(a, a2);
    mnd_md_node_append_child(root, b);
    mnd_md_node_append_child(root, c);
    mnd_md_node_append_child(c, c1);

    expect[0] = root;
    expect[1] = a;
    expect[2] = a1;
    expect[3] = a1x;
    expect[4] = a2;
    expect[5] = b;
    expect[6] = c;
    expect[7] = c1;
    it = root;
    for (i = 0; i < 8; i++)
    {
        NAOS_REQUIRE_TRUE(it == expect[i]);
        it = mnd_md_node_next(it, root);
    }
    NAOS_CHECK_NULL(it);
    /* 部分木 a の走査は a の兄弟（b）に出ない */
    it = a;
    for (i = 1; i < 5; i++)
    {
        NAOS_REQUIRE_TRUE(it == expect[i]);
        it = mnd_md_node_next(it, a);
    }
    NAOS_CHECK_NULL(it);
    /* 子のないノードを根にすると、次はない */
    NAOS_CHECK_NULL(mnd_md_node_next(b, b));
    NAOS_CHECK_NULL(mnd_md_node_next(a1x, a1));
    /* 最後のノードの次はない */
    NAOS_CHECK_NULL(mnd_md_node_next(c1, root));
    mnd_md_doc_free(doc);
}

NAOS_TEST(deep_tree_traversal_and_free_do_not_crash)
{
    mnd_md_test_alloc_t ta;
    mnd_md_doc_t *doc = NULL;
    mnd_md_node_t *parent = NULL;
    mnd_md_node_t *deepest = NULL;
    const mnd_md_node_t *it = NULL;
    size_t i = 0;
    size_t visited = 0;

    mnd_md_test_alloc_init(&ta, 0);
    doc = mnd_md_doc_create("x", 1, &ta.allocator);
    NAOS_REQUIRE_NOT_NULL(doc);
    parent = doc->root;
    for (i = 0; i < DEEP_TREE_DEPTH; i++)
    {
        mnd_md_node_t *node = mnd_md_node_new(doc, MND_MD_BLOCK_QUOTE, span_of(0, 1));

        NAOS_REQUIRE_NOT_NULL(node);
        mnd_md_node_append_child(parent, node);
        parent = node;
    }
    deepest = parent;
    /* 最も深いノードに兄弟を足し、深いところから根まで戻って抜ける経路も通す */
    {
        mnd_md_node_t *sibling = mnd_md_node_new(doc, MND_MD_TEXT, span_of(0, 1));

        NAOS_REQUIRE_NOT_NULL(sibling);
        mnd_md_node_insert_after(deepest, sibling);
    }
    for (it = doc->root; it != NULL; it = mnd_md_node_next(it, doc->root))
    {
        visited++;
    }
    NAOS_CHECK_UINT_EQ(DEEP_TREE_DEPTH + 2, visited);
    /* 最も深いノードを根にした部分木はそれ自身だけ */
    NAOS_CHECK_NULL(mnd_md_node_next(deepest, deepest));
    mnd_md_doc_free(doc);
    check_balanced(&ta);
}

NAOS_TEST(wide_tree_traversal_and_free)
{
    mnd_md_test_alloc_t ta;
    mnd_md_doc_t *doc = NULL;
    const mnd_md_node_t *it = NULL;
    size_t i = 0;
    size_t visited = 0;

    mnd_md_test_alloc_init(&ta, 0);
    doc = mnd_md_doc_create("x", 1, &ta.allocator);
    NAOS_REQUIRE_NOT_NULL(doc);
    for (i = 0; i < DEEP_TREE_DEPTH; i++)
    {
        mnd_md_node_t *node = mnd_md_node_new(doc, MND_MD_TEXT, span_of(0, 1));

        NAOS_REQUIRE_NOT_NULL(node);
        mnd_md_node_append_child(doc->root, node);
    }
    for (it = doc->root; it != NULL; it = mnd_md_node_next(it, doc->root))
    {
        visited++;
    }
    NAOS_CHECK_UINT_EQ(DEEP_TREE_DEPTH + 1, visited);
    mnd_md_doc_free(doc);
    check_balanced(&ta);
}

NAOS_SUITE(model,
    NAOS_ADD_TEST(model_kind_dialect_starts_after_commonmark_kinds),
    NAOS_ADD_TEST(test_alloc_counts_allocs_and_frees),
    NAOS_ADD_TEST(test_alloc_fails_only_at_nth_alloc),
    NAOS_ADD_TEST(null_allocator_defaults_to_malloc_and_free),
    NAOS_ADD_TEST(size_mul_detects_overflow),
    NAOS_ADD_TEST(arena_allocations_are_aligned_and_zeroed),
    NAOS_ADD_TEST(arena_big_and_many_allocations),
    NAOS_ADD_TEST(arena_dup_and_array),
    NAOS_ADD_TEST(arena_refuses_overflowing_sizes_before_allocating),
    NAOS_ADD_TEST(arena_reports_allocation_failure),
    NAOS_ADD_TEST(work_array_grows_and_keeps_contents),
    NAOS_ADD_TEST(work_array_reports_allocation_failure),
    NAOS_ADD_TEST(work_array_refuses_overflowing_capacity),
    NAOS_ADD_TEST(work_array_reserve_zero_and_default_allocator),
    NAOS_ADD_TEST(doc_create_and_accessors),
    NAOS_ADD_TEST(doc_empty_content_and_default_allocator),
    NAOS_ADD_TEST(doc_free_and_accessors_accept_null),
    NAOS_ADD_TEST(doc_create_reports_allocation_failure),
    NAOS_ADD_TEST(node_new_sets_kind_and_span),
    NAOS_ADD_TEST(node_new_reports_allocation_failure),
    NAOS_ADD_TEST(append_child_keeps_order),
    NAOS_ADD_TEST(insert_before_and_after_keep_links),
    NAOS_ADD_TEST(unlink_removes_first_middle_last_and_only),
    NAOS_ADD_TEST(node_next_is_preorder_and_bounded_to_subtree),
    NAOS_ADD_TEST(deep_tree_traversal_and_free_do_not_crash),
    NAOS_ADD_TEST(wide_tree_traversal_and_free)
)
