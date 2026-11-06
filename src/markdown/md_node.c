/* Model: ノードの作成と連結、文書の解放、走査 */

#include "markdown/md.h"
#include "markdown/md_internal.h"

#include <string.h>

/* 文書 */

mnd_md_doc_t *mnd_md_doc_create(const char *data, size_t len, const mnd_md_allocator_t *allocator)
{
    const mnd_md_allocator_t *a = mnd_md_allocator_or_default(allocator);
    mnd_md_doc_t *doc = NULL;
    mnd_span_t span;

    doc = (mnd_md_doc_t *)a->alloc(a->ctx, sizeof(*doc));
    if (doc == NULL)
    {
        return NULL;
    }

    memset(doc, 0, sizeof(*doc));
    doc->data = data;
    doc->len = len;
    mnd_md_arena_init(&doc->arena, a);
    span.start = 0;
    span.end = len;
    doc->root = mnd_md_node_new(doc, MND_MD_DOCUMENT, span);
    if (doc->root == NULL)
    {
        mnd_md_doc_free(doc);

        return NULL;
    }

    return doc;
}

const mnd_md_node_t *mnd_md_doc_root(const mnd_md_doc_t *doc)
{
    if (doc == NULL)
    {
        return NULL;
    }

    return doc->root;
}

const char *mnd_md_doc_data(const mnd_md_doc_t *doc)
{
    if (doc == NULL)
    {
        return NULL;
    }

    return doc->data;
}

size_t mnd_md_doc_len(const mnd_md_doc_t *doc)
{
    if (doc == NULL)
    {
        return 0;
    }

    return doc->len;
}

void mnd_md_doc_free(mnd_md_doc_t *doc)
{
    mnd_md_allocator_t allocator;

    if (doc == NULL)
    {
        return;
    }

    /* ノードはすべてアリーナにあるので、木をたどらずに塊をまとめて解放する（深さに依存しない） */
    allocator = doc->arena.allocator;
    mnd_md_arena_free(&doc->arena);
    allocator.free(allocator.ctx, doc);
}

/* ノードの作成と連結 */

mnd_md_node_t *mnd_md_node_new(mnd_md_doc_t *doc, mnd_md_kind_t kind, mnd_span_t span)
{
    mnd_md_node_t *node = (mnd_md_node_t *)mnd_md_arena_alloc(&doc->arena, sizeof(*node));

    if (node == NULL)
    {
        return NULL;
    }

    /* アリーナは 0 で埋める。ポインタの欄は明示的に NULL にする */
    node->kind = kind;
    node->span = span;
    node->parent = NULL;
    node->first_child = NULL;
    node->last_child = NULL;
    node->prev = NULL;
    node->next = NULL;
    node->lines = NULL;

    return node;
}

void mnd_md_node_unlink(mnd_md_node_t *node)
{
    mnd_md_node_t *parent = node->parent;

    if (node->prev != NULL)
    {
        node->prev->next = node->next;
    }
    else if (parent != NULL)
    {
        parent->first_child = node->next;
    }

    if (node->next != NULL)
    {
        node->next->prev = node->prev;
    }
    else if (parent != NULL)
    {
        parent->last_child = node->prev;
    }

    node->parent = NULL;
    node->prev = NULL;
    node->next = NULL;
}

void mnd_md_node_append_child(mnd_md_node_t *parent, mnd_md_node_t *node)
{
    mnd_md_node_unlink(node);
    node->parent = parent;
    node->prev = parent->last_child;

    if (parent->last_child != NULL)
    {
        parent->last_child->next = node;
    }
    else
    {
        parent->first_child = node;
    }

    parent->last_child = node;
}

void mnd_md_node_insert_before(mnd_md_node_t *sibling, mnd_md_node_t *node)
{
    if (node == sibling)
    {
        return;
    }

    mnd_md_node_unlink(node);
    node->parent = sibling->parent;
    node->prev = sibling->prev;
    node->next = sibling;

    if (sibling->prev != NULL)
    {
        sibling->prev->next = node;
    }
    else if (sibling->parent != NULL)
    {
        sibling->parent->first_child = node;
    }

    sibling->prev = node;
}

void mnd_md_node_insert_after(mnd_md_node_t *sibling, mnd_md_node_t *node)
{
    if (node == sibling)
    {
        return;
    }

    mnd_md_node_unlink(node);
    node->parent = sibling->parent;
    node->prev = sibling;
    node->next = sibling->next;

    if (sibling->next != NULL)
    {
        sibling->next->prev = node;
    }
    else if (sibling->parent != NULL)
    {
        sibling->parent->last_child = node;
    }

    sibling->next = node;
}

/* 走査 */

const mnd_md_node_t *mnd_md_node_next(const mnd_md_node_t *node, const mnd_md_node_t *root)
{
    /* 子があれば最初の子。なければ、root に戻るまで、自分か祖先の次の兄弟を探す（再帰しない） */
    if (node == NULL)
    {
        return NULL;
    }

    if (node->first_child != NULL)
    {
        return node->first_child;
    }

    while ((node != NULL) && (node != root))
    {
        if (node->next != NULL)
        {
            return node->next;
        }

        node = node->parent;
    }

    return NULL;
}
