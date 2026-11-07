/* スイート model: 構文木の型、アリーナ、文書のアクセサと解放（2.6、3.1、6.7、6.10）と、テスト用の確保関数 */

#include <naos/naos.h>

#include "test_alloc.h"
#include "tree_check.h"
#include "yaml/yaml.h"
#include "yaml/yaml_internal.h"

#include <stdint.h>
#include <stddef.h>

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

NAOS_SUITE(model,
    NAOS_ADD_TEST(test_alloc_counts_allocs_and_frees),
    NAOS_ADD_TEST(test_alloc_fails_only_at_nth_alloc),
    NAOS_ADD_TEST(model_scaffold_default_allocator_and_size_mul)
)
