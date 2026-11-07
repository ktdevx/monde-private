/* Model: 確保関数の既定値、アリーナ、ノードの作成、文書のアクセサと解放 */

#include "yaml/yaml.h"
#include "yaml/yaml_internal.h"

#include <stdint.h>
#include <stdlib.h>

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
 * 以下のアリーナ・文書・ノードの関数の本体はタスク 1.2 が実装する。
 * それまでは確保せずに失敗（NULL、false）を返す
 */

mnd_yaml_doc_t *mnd_yaml_doc_new(const char *data, size_t len, mnd_span_t content, mnd_eol_t eol,
                                 const mnd_yaml_allocator_t *allocator)
{
    (void)data;
    (void)len;
    (void)content;
    (void)eol;
    (void)allocator;

    return NULL;
}

void mnd_yaml_doc_set_root(mnd_yaml_doc_t *doc, mnd_yaml_node_t *node)
{
    (void)doc;
    (void)node;
}

void *mnd_yaml_arena_alloc(mnd_yaml_doc_t *doc, size_t size)
{
    (void)doc;
    (void)size;

    return NULL;
}

mnd_yaml_node_t *mnd_yaml_node_new(mnd_yaml_doc_t *doc, mnd_yaml_kind_t kind)
{
    (void)doc;
    (void)kind;

    return NULL;
}

char *mnd_yaml_arena_strdup(mnd_yaml_doc_t *doc, const char *data, size_t len)
{
    (void)doc;
    (void)data;
    (void)len;

    return NULL;
}

bool mnd_yaml_key_equal(const mnd_yaml_node_t *a, const mnd_yaml_node_t *b)
{
    (void)a;
    (void)b;

    return false;
}

const mnd_yaml_node_t *mnd_yaml_doc_root(const mnd_yaml_doc_t *doc)
{
    (void)doc;

    return NULL;
}

const char *mnd_yaml_doc_data(const mnd_yaml_doc_t *doc)
{
    (void)doc;

    return NULL;
}

size_t mnd_yaml_doc_len(const mnd_yaml_doc_t *doc)
{
    (void)doc;

    return 0;
}

mnd_span_t mnd_yaml_doc_content(const mnd_yaml_doc_t *doc)
{
    mnd_span_t span;

    (void)doc;
    span.start = 0;
    span.end = 0;

    return span;
}

mnd_eol_t mnd_yaml_doc_eol(const mnd_yaml_doc_t *doc)
{
    (void)doc;

    return MND_EOL_LF;
}

void mnd_yaml_doc_free(mnd_yaml_doc_t *doc)
{
    (void)doc;
}
