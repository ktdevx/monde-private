/* Model: 確保関数の既定値、アリーナ、ノードの作成、文書のアクセサと解放 */

#include "yaml/yaml.h"
#include "yaml/yaml_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static const mnd_yaml_allocator_t default_allocator = { default_alloc, default_free, NULL };

const mnd_yaml_allocator_t *mnd_yaml_allocator_or_default(const mnd_yaml_allocator_t *allocator)
{
    if (allocator == NULL)
    {
        return &default_allocator;
    }

    return allocator;
}

bool mnd_yaml_size_mul(size_t count, size_t elem_size, size_t *out)
{
    if ((elem_size != 0) && (count > (SIZE_MAX / elem_size)))
    {
        return false;
    }
    *out = count * elem_size;

    return true;
}

/*
 * どの型にも使える境界。C99 には max_align_t がないため、境界の厳しい型を集めた共用体の大きさを
 * 切り出しの単位にする（共用体の大きさは、各要素の境界の倍数である）
 */
typedef union mnd_yaml_max_align
{
    long double ld;
    double d;
    long long ll;
    void *p;
    void (*fp)(void);
} mnd_yaml_max_align_t;

#define MND_YAML_ARENA_ALIGN (sizeof(mnd_yaml_max_align_t))

/* 塊のデータ部の既定のバイト数 */
#define MND_YAML_ARENA_CHUNK_SIZE ((size_t)8192)

/* これより大きい確保は専用の塊にする（切り出し中の塊の残りを無駄にしない） */
#define MND_YAML_ARENA_BIG_SIZE (MND_YAML_ARENA_CHUNK_SIZE / 4)

/* 塊の見出し。データ部は見出しの直後（境界に揃えた位置）から始まる */
struct mnd_yaml_arena_chunk
{
    mnd_yaml_arena_chunk_t *next;
    size_t cap;     /* データ部のバイト数 */
    size_t used;    /* 切り出したバイト数 */
};

/* 見出しの大きさを境界の倍数に切り上げたもの */
#define MND_YAML_ARENA_HEADER_SIZE \
    (((sizeof(mnd_yaml_arena_chunk_t) + MND_YAML_ARENA_ALIGN - 1) / MND_YAML_ARENA_ALIGN) \
     * MND_YAML_ARENA_ALIGN)

static unsigned char *chunk_data(mnd_yaml_arena_chunk_t *chunk)
{
    return (unsigned char *)chunk + MND_YAML_ARENA_HEADER_SIZE;
}

/* データ部が cap バイトの塊を確保する。失敗したら NULL */
static mnd_yaml_arena_chunk_t *chunk_new(mnd_yaml_doc_t *doc, size_t cap)
{
    mnd_yaml_arena_chunk_t *chunk = NULL;

    if (cap > (SIZE_MAX - MND_YAML_ARENA_HEADER_SIZE))
    {
        return NULL;
    }
    chunk = (mnd_yaml_arena_chunk_t *)doc->allocator.alloc(doc->allocator.ctx,
                                                           MND_YAML_ARENA_HEADER_SIZE + cap);
    if (chunk == NULL)
    {
        return NULL;
    }
    chunk->next = NULL;
    chunk->cap = cap;
    chunk->used = 0;

    return chunk;
}

mnd_yaml_doc_t *mnd_yaml_doc_new(const char *data, size_t len, mnd_span_t content, mnd_eol_t eol,
                                 const mnd_yaml_allocator_t *allocator)
{
    const mnd_yaml_allocator_t *resolved = mnd_yaml_allocator_or_default(allocator);
    mnd_yaml_doc_t *doc = NULL;

    doc = (mnd_yaml_doc_t *)resolved->alloc(resolved->ctx, sizeof(*doc));
    if (doc == NULL)
    {
        return NULL;
    }
    doc->data = data;
    doc->len = len;
    doc->content = content;
    doc->eol = (eol == MND_EOL_CRLF) ? MND_EOL_CRLF : MND_EOL_LF;
    doc->root = NULL;
    doc->allocator = *resolved;
    doc->chunks = NULL;

    return doc;
}

void mnd_yaml_doc_set_root(mnd_yaml_doc_t *doc, mnd_yaml_node_t *node)
{
    doc->root = node;
}

void *mnd_yaml_arena_alloc(mnd_yaml_doc_t *doc, size_t size)
{
    mnd_yaml_arena_chunk_t *head = doc->chunks;
    mnd_yaml_arena_chunk_t *chunk = NULL;
    unsigned char *ptr = NULL;
    size_t rounded = 0;

    if (size > (SIZE_MAX - (MND_YAML_ARENA_ALIGN - 1)))
    {
        return NULL;
    }
    rounded = ((size + (MND_YAML_ARENA_ALIGN - 1)) / MND_YAML_ARENA_ALIGN) * MND_YAML_ARENA_ALIGN;
    if (rounded == 0)
    {
        rounded = MND_YAML_ARENA_ALIGN;
    }

    if ((head != NULL) && ((head->cap - head->used) >= rounded))
    {
        ptr = chunk_data(head) + head->used;
        head->used += rounded;
        memset(ptr, 0, rounded);

        return ptr;
    }

    if (rounded > MND_YAML_ARENA_BIG_SIZE)
    {
        /* 専用の塊。切り出し中の塊はそのまま先頭に残す */
        chunk = chunk_new(doc, rounded);
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
            doc->chunks = chunk;
        }
        ptr = chunk_data(chunk);
        memset(ptr, 0, rounded);

        return ptr;
    }

    chunk = chunk_new(doc, MND_YAML_ARENA_CHUNK_SIZE);
    if (chunk == NULL)
    {
        return NULL;
    }
    chunk->next = head;
    doc->chunks = chunk;
    chunk->used = rounded;
    ptr = chunk_data(chunk);
    memset(ptr, 0, rounded);

    return ptr;
}

mnd_yaml_node_t *mnd_yaml_node_new(mnd_yaml_doc_t *doc, mnd_yaml_kind_t kind)
{
    mnd_yaml_node_t *node = (mnd_yaml_node_t *)mnd_yaml_arena_alloc(doc, sizeof(*node));

    if (node == NULL)
    {
        return NULL;
    }
    /*
     * 0 で埋めたメモリのポインタが NULL とは限らない（C99 の規定）ため、ポインタと列挙値は明示的に設定する
     */
    node->kind = kind;
    node->span.start = 0;
    node->span.end = 0;
    node->start_line = 0;
    node->end_line = 0;
    node->parent = NULL;
    if (kind == MND_YAML_MAPPING)
    {
        node->as.mapping.pairs = NULL;
        node->as.mapping.count = 0;
    }
    else if (kind == MND_YAML_SEQUENCE)
    {
        node->as.sequence.items = NULL;
        node->as.sequence.count = 0;
    }
    else
    {
        node->as.scalar.style = MND_YAML_PLAIN;
        node->as.scalar.value = NULL;
        node->as.scalar.value_len = 0;
        node->as.scalar.flow = false;
        node->as.scalar.chomp = MND_YAML_CHOMP_CLIP;
        node->as.scalar.has_indent_indicator = false;
        node->as.scalar.indent_indicator = 0;
        node->as.scalar.content_indent = 0;
    }

    return node;
}

char *mnd_yaml_arena_strdup(mnd_yaml_doc_t *doc, const char *data, size_t len)
{
    char *copy = NULL;

    if (len == SIZE_MAX)
    {
        return NULL;
    }
    copy = (char *)mnd_yaml_arena_alloc(doc, len + 1);
    if (copy == NULL)
    {
        return NULL;
    }
    if (len > 0)
    {
        memcpy(copy, data, len);
    }
    copy[len] = '\0';

    return copy;
}

bool mnd_yaml_key_equal(const mnd_yaml_node_t *a, const mnd_yaml_node_t *b)
{
    if ((a == NULL) || (b == NULL) || (a->kind != MND_YAML_SCALAR) || (b->kind != MND_YAML_SCALAR))
    {
        return false;
    }
    if (a->as.scalar.value_len != b->as.scalar.value_len)
    {
        return false;
    }
    if (a->as.scalar.value_len == 0)
    {
        return true;
    }
    if ((a->as.scalar.value == NULL) || (b->as.scalar.value == NULL))
    {
        return false;
    }

    return memcmp(a->as.scalar.value, b->as.scalar.value, a->as.scalar.value_len) == 0;
}

const mnd_yaml_node_t *mnd_yaml_doc_root(const mnd_yaml_doc_t *doc)
{
    return (doc == NULL) ? NULL : doc->root;
}

const char *mnd_yaml_doc_data(const mnd_yaml_doc_t *doc)
{
    return (doc == NULL) ? NULL : doc->data;
}

size_t mnd_yaml_doc_len(const mnd_yaml_doc_t *doc)
{
    return (doc == NULL) ? 0 : doc->len;
}

mnd_span_t mnd_yaml_doc_content(const mnd_yaml_doc_t *doc)
{
    mnd_span_t span;

    span.start = 0;
    span.end = 0;
    if (doc != NULL)
    {
        span = doc->content;
    }

    return span;
}

mnd_eol_t mnd_yaml_doc_eol(const mnd_yaml_doc_t *doc)
{
    return (doc == NULL) ? MND_EOL_LF : doc->eol;
}

void mnd_yaml_doc_free(mnd_yaml_doc_t *doc)
{
    mnd_yaml_allocator_t allocator;
    mnd_yaml_arena_chunk_t *chunk = NULL;

    if (doc == NULL)
    {
        return;
    }
    /* 文書の構造体を解放するまで確保関数を使うので、先に複製する */
    allocator = doc->allocator;
    chunk = doc->chunks;
    while (chunk != NULL)
    {
        mnd_yaml_arena_chunk_t *next = chunk->next;

        allocator.free(allocator.ctx, chunk);
        chunk = next;
    }
    doc->chunks = NULL;
    allocator.free(allocator.ctx, doc);
}
