/* test_alloc: n 回目の確保で失敗し、確保と解放の回数を数える確保関数 */

#include "test_alloc.h"

#include <stdlib.h>

static void *test_alloc_alloc(void *ctx, size_t size)
{
    mnd_yaml_test_alloc_t *ta = (mnd_yaml_test_alloc_t *)ctx;
    void *ptr = NULL;

    ta->calls++;
    if ((ta->fail_at != 0) && (ta->calls == ta->fail_at))
    {
        ta->failures++;

        return NULL;
    }
    /* malloc(0) が NULL を返す環境でも、成功と失敗を区別できるようにする */
    ptr = malloc((size == 0) ? 1 : size);
    if (ptr == NULL)
    {
        ta->failures++;

        return NULL;
    }
    ta->allocs++;

    return ptr;
}

static void test_alloc_free(void *ctx, void *ptr)
{
    mnd_yaml_test_alloc_t *ta = (mnd_yaml_test_alloc_t *)ctx;

    if (ptr == NULL)
    {
        return;
    }
    ta->frees++;
    free(ptr);
}

void mnd_yaml_test_alloc_init(mnd_yaml_test_alloc_t *ta, size_t fail_at)
{
    ta->allocator.alloc = test_alloc_alloc;
    ta->allocator.free = test_alloc_free;
    ta->allocator.ctx = ta;
    ta->fail_at = fail_at;
    ta->calls = 0;
    ta->allocs = 0;
    ta->frees = 0;
    ta->failures = 0;
}

size_t mnd_yaml_test_alloc_live(const mnd_yaml_test_alloc_t *ta)
{
    return ta->allocs - ta->frees;
}
