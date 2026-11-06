/* Model: アリーナと、確保関数の既定値 */

#include "markdown/md.h"
#include "markdown/md_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * どの型にも使える境界。C99 には max_align_t がないため、境界の厳しい型を集めた共用体の大きさを
 * 切り出しの単位にする（共用体の大きさは、各要素の境界の倍数である）
 */
typedef union mnd_md_max_align
{
    long double ld;
    double d;
    long long ll;
    void *p;
    void (*fp)(void);
} mnd_md_max_align_t;

#define MND_MD_ARENA_ALIGN (sizeof(mnd_md_max_align_t))

/* 塊のデータ部の既定のバイト数 */
#define MND_MD_ARENA_CHUNK_SIZE ((size_t)16384)

/* これより大きい確保は専用の塊にする（切り出し中の塊の残りを無駄にしない） */
#define MND_MD_ARENA_BIG_SIZE (MND_MD_ARENA_CHUNK_SIZE / 4)

/* 作業用の配列の最初の容量 */
#define MND_MD_ARRAY_MIN_CAP ((size_t)8)

/* 塊の見出し。データ部は見出しの直後（境界に揃えた位置）から始まる */
struct mnd_md_arena_chunk
{
    mnd_md_arena_chunk_t *next;
    size_t cap;     /* データ部のバイト数 */
    size_t used;    /* 切り出したバイト数 */
};

/* 見出しの大きさを境界の倍数に切り上げたもの */
#define MND_MD_ARENA_HEADER_SIZE \
    (((sizeof(mnd_md_arena_chunk_t) + MND_MD_ARENA_ALIGN - 1) / MND_MD_ARENA_ALIGN) * MND_MD_ARENA_ALIGN)

static void *default_alloc(void *ctx, size_t size)
{
    (void)ctx;

    return malloc((size == 0) ? 1 : size);
}

static void default_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

static const mnd_md_allocator_t default_allocator = { default_alloc, default_free, NULL };

const mnd_md_allocator_t *mnd_md_allocator_or_default(const mnd_md_allocator_t *allocator)
{
    if (allocator == NULL)
    {
        return &default_allocator;
    }

    return allocator;
}

bool mnd_md_size_mul(size_t count, size_t elem_size, size_t *out)
{
    if ((elem_size != 0) && (count > (SIZE_MAX / elem_size)))
    {
        return false;
    }

    *out = count * elem_size;

    return true;
}

/* アリーナ */

static unsigned char *chunk_data(mnd_md_arena_chunk_t *chunk)
{
    return (unsigned char *)chunk + MND_MD_ARENA_HEADER_SIZE;
}

/* データ部が cap バイトの塊を確保する。失敗したら NULL */
static mnd_md_arena_chunk_t *chunk_new(mnd_md_arena_t *arena, size_t cap)
{
    mnd_md_arena_chunk_t *chunk = NULL;

    if (cap > (SIZE_MAX - MND_MD_ARENA_HEADER_SIZE))
    {
        return NULL;
    }

    chunk = (mnd_md_arena_chunk_t *)arena->allocator.alloc(arena->allocator.ctx,
                                                           MND_MD_ARENA_HEADER_SIZE + cap);
    if (chunk == NULL)
    {
        return NULL;
    }

    chunk->next = NULL;
    chunk->cap = cap;
    chunk->used = 0;

    return chunk;
}

void mnd_md_arena_init(mnd_md_arena_t *arena, const mnd_md_allocator_t *allocator)
{
    arena->allocator = *mnd_md_allocator_or_default(allocator);
    arena->chunks = NULL;
}

void *mnd_md_arena_alloc(mnd_md_arena_t *arena, size_t size)
{
    mnd_md_arena_chunk_t *head = arena->chunks;
    mnd_md_arena_chunk_t *chunk = NULL;
    unsigned char *ptr = NULL;
    size_t rounded = 0;

    if (size > (SIZE_MAX - (MND_MD_ARENA_ALIGN - 1)))
    {
        return NULL;
    }

    rounded = ((size + (MND_MD_ARENA_ALIGN - 1)) / MND_MD_ARENA_ALIGN) * MND_MD_ARENA_ALIGN;
    if (rounded == 0)
    {
        rounded = MND_MD_ARENA_ALIGN;
    }

    if ((head != NULL) && ((head->cap - head->used) >= rounded))
    {
        ptr = chunk_data(head) + head->used;
        head->used += rounded;
        memset(ptr, 0, size);

        return ptr;
    }

    if (rounded > MND_MD_ARENA_BIG_SIZE)
    {
        /* 専用の塊。切り出し中の塊はそのまま先頭に残す */
        chunk = chunk_new(arena, rounded);
        if (chunk == NULL)
        {
            return NULL;
        }
    
        chunk->used = rounded;
    
        if (head != NULL)
        {
            chunk->next = head->next;
            head->next = chunk;
        }
        else
        {
            arena->chunks = chunk;
        }

        ptr = chunk_data(chunk);
        memset(ptr, 0, size);

        return ptr;
    }

    chunk = chunk_new(arena, MND_MD_ARENA_CHUNK_SIZE);
    if (chunk == NULL)
    {
        return NULL;
    }

    chunk->next = head;
    arena->chunks = chunk;
    chunk->used = rounded;
    ptr = chunk_data(chunk);
    memset(ptr, 0, size);

    return ptr;
}

void *mnd_md_arena_alloc_array(mnd_md_arena_t *arena, size_t count, size_t elem_size)
{
    size_t size = 0;

    if (!mnd_md_size_mul(count, elem_size, &size))
    {
        return NULL;
    }

    return mnd_md_arena_alloc(arena, size);
}

void *mnd_md_arena_dup(mnd_md_arena_t *arena, const void *src, size_t size)
{
    void *ptr = mnd_md_arena_alloc(arena, size);

    if ((ptr != NULL) && (size > 0))
    {
        memcpy(ptr, src, size);
    }

    return ptr;
}

void mnd_md_arena_free(mnd_md_arena_t *arena)
{
    mnd_md_arena_chunk_t *chunk = arena->chunks;

    while (chunk != NULL)
    {
        mnd_md_arena_chunk_t *next = chunk->next;

        arena->allocator.free(arena->allocator.ctx, chunk);
        chunk = next;
    }

    arena->chunks = NULL;
}

/* 作業用の配列 */

void mnd_md_array_init(mnd_md_array_t *array, size_t elem_size,
                       const mnd_md_allocator_t *allocator)
{
    array->allocator = *mnd_md_allocator_or_default(allocator);
    array->items = NULL;
    array->elem_size = elem_size;
    array->count = 0;
    array->cap = 0;
}

bool mnd_md_array_reserve(mnd_md_array_t *array, size_t additional)
{
    size_t needed = 0;
    size_t new_cap = 0;
    size_t bytes = 0;
    size_t old_bytes = 0;
    void *items = NULL;

    if (additional > (SIZE_MAX - array->count))
    {
        return false;
    }

    needed = array->count + additional;
    if (needed <= array->cap)
    {
        return true;
    }

    new_cap = (array->cap == 0) ? MND_MD_ARRAY_MIN_CAP : array->cap;
    while (new_cap < needed)
    {
        if (new_cap > (SIZE_MAX / 2))
        {
            new_cap = needed;
        }
        else
        {
            new_cap *= 2;
        }
    }

    if (!mnd_md_size_mul(new_cap, array->elem_size, &bytes))
    {
        return false;
    }

    items = array->allocator.alloc(array->allocator.ctx, bytes);
    if (items == NULL)
    {
        return false;
    }

    /* count * elem_size は、確保済みの容量の範囲なのでオーバーフローしない */
    old_bytes = array->count * array->elem_size;
    if (old_bytes > 0)
    {
        memcpy(items, array->items, old_bytes);
    }

    array->allocator.free(array->allocator.ctx, array->items);
    array->items = items;
    array->cap = new_cap;

    return true;
}

void *mnd_md_array_push(mnd_md_array_t *array)
{
    unsigned char *slot = NULL;

    if (!mnd_md_array_reserve(array, 1))
    {
        return NULL;
    }

    slot = (unsigned char *)array->items + (array->count * array->elem_size);
    memset(slot, 0, array->elem_size);
    array->count++;

    return slot;
}

bool mnd_md_array_append(mnd_md_array_t *array, const void *elem)
{
    void *slot = mnd_md_array_push(array);

    if (slot == NULL)
    {
        return false;
    }

    memcpy(slot, elem, array->elem_size);

    return true;
}

void *mnd_md_array_at(const mnd_md_array_t *array, size_t index)
{
    if (index >= array->count)
    {
        return NULL;
    }

    return (unsigned char *)array->items + (index * array->elem_size);
}

void *mnd_md_array_last(const mnd_md_array_t *array)
{
    if (array->count == 0)
    {
        return NULL;
    }

    return mnd_md_array_at(array, array->count - 1);
}

void mnd_md_array_pop(mnd_md_array_t *array)
{
    if (array->count > 0)
    {
        array->count--;
    }
}

void mnd_md_array_clear(mnd_md_array_t *array)
{
    array->count = 0;
}

void mnd_md_array_free(mnd_md_array_t *array)
{
    array->allocator.free(array->allocator.ctx, array->items);
    array->items = NULL;
    array->count = 0;
    array->cap = 0;
}
