/* Path: キーパスの検証と照合、該当するノードの列 */

#include "yaml/yaml_path.h"
#include "yaml/yaml_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* 作業用の配列の最初の容量 */
#define PATH_LIST_INITIAL_CAP 8

/*
 * ノードへのポインタの伸びる配列。確保関数から直接確保する（アリーナは使わない。文書は const のため）
 */
typedef struct path_list
{
    const mnd_yaml_node_t **items;
    size_t count;
    size_t cap;
} path_list_t;

static void path_list_free(const mnd_yaml_allocator_t *allocator, path_list_t *list)
{
    if (list->items != NULL)
    {
        allocator->free(allocator->ctx, (void *)list->items);
    }
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

/*
 * list の末尾に node を追加する。メモリ不足または大きさのオーバーフローなら false（list は変更しない）
 */
static bool path_list_push(const mnd_yaml_allocator_t *allocator, path_list_t *list, const mnd_yaml_node_t *node)
{
    const mnd_yaml_node_t **items = NULL;
    size_t new_cap;
    size_t bytes;

    if (list->count == list->cap)
    {
        if (list->cap == 0)
        {
            new_cap = PATH_LIST_INITIAL_CAP;
        }
        else
        {
            if (list->cap > (SIZE_MAX / 2))
            {
                return false;
            }
            new_cap = list->cap * 2;
        }
        if (!mnd_yaml_size_mul(new_cap, sizeof(*items), &bytes))
        {
            return false;
        }
        items = (const mnd_yaml_node_t **)allocator->alloc(allocator->ctx, bytes);
        if (items == NULL)
        {
            return false;
        }
        if (list->count > 0)
        {
            memcpy((void *)items, (const void *)list->items, list->count * sizeof(*items));
        }
        if (list->items != NULL)
        {
            allocator->free(allocator->ctx, (void *)list->items);
        }
        list->items = items;
        list->cap = new_cap;
    }
    list->items[list->count] = node;
    ++list->count;

    return true;
}

/*
 * キーパスが空でなく、空のセグメント（先頭・末尾の `.`、連続する `.`）を含まないか
 */
static bool path_is_valid(const char *path, size_t path_len)
{
    size_t segment_len = 0;
    size_t i;

    if ((path == NULL) || (path_len == 0))
    {
        return false;
    }
    for (i = 0; i < path_len; ++i)
    {
        if (path[i] == '.')
        {
            if (segment_len == 0)
            {
                return false;
            }
            segment_len = 0;
        }
        else
        {
            ++segment_len;
        }
    }

    return (segment_len > 0);
}

/*
 * node（とその中のシーケンスの入れ子）を、明示的なスタックで原文の順にたどる。
 * key が NULL でなければ、たどった先のマッピングで key と論理値が一致するキーの値を out に追加する
 * （スカラーは脱落）。key が NULL なら、シーケンスを要素に展開し、シーケンス以外のノードを out に追加する。
 * メモリ不足なら false
 */
static bool path_visit(const mnd_yaml_allocator_t *allocator, const mnd_yaml_node_t *node,
                       const mnd_yaml_node_t *key, path_list_t *stack, path_list_t *out)
{
    const mnd_yaml_node_t *n = NULL;
    size_t i;

    stack->count = 0;
    if (!path_list_push(allocator, stack, node))
    {
        return false;
    }
    while (stack->count > 0)
    {
        --stack->count;
        n = stack->items[stack->count];
        if (n == NULL)
        {
            continue;
        }
        if (n->kind == MND_YAML_SEQUENCE)
        {
            /*
             * 先頭の要素が先に取り出されるよう、逆順に積む
             */
            for (i = n->as.sequence.count; i > 0; --i)
            {
                if (!path_list_push(allocator, stack, n->as.sequence.items[i - 1]))
                {
                    return false;
                }
            }
        }
        else if (key == NULL)
        {
            if (!path_list_push(allocator, out, n))
            {
                return false;
            }
        }
        else if (n->kind == MND_YAML_MAPPING)
        {
            for (i = 0; i < n->as.mapping.count; ++i)
            {
                if (mnd_yaml_key_equal(n->as.mapping.pairs[i].key, key)
                    && (n->as.mapping.pairs[i].value != NULL))
                {
                    if (!path_list_push(allocator, out, n->as.mapping.pairs[i].value))
                    {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

/*
 * 検証済みのキーパスで照合し、結果を result に入れる。メモリ不足なら false（作業用の配列は解放済み、
 * result は空）
 */
static bool path_match(const mnd_yaml_doc_t *doc, const char *path, size_t path_len, path_list_t *result)
{
    const mnd_yaml_allocator_t *allocator = &doc->allocator;
    const mnd_yaml_node_t *root = doc->root;
    path_list_t current = { NULL, 0, 0 };
    path_list_t next = { NULL, 0, 0 };
    path_list_t stack = { NULL, 0, 0 };
    path_list_t swap;
    mnd_yaml_node_t key;
    size_t start = 0;
    size_t end;
    size_t i;
    bool ok = true;

    result->items = NULL;
    result->count = 0;
    result->cap = 0;
    if ((root == NULL) || (root->kind != MND_YAML_MAPPING))
    {
        return true;
    }
    if (!path_list_push(allocator, &current, root))
    {
        return false;
    }

    /*
     * セグメントの論理値と比べるための一時的なスカラーのノード（mnd_yaml_key_equal で比べる）
     */
    memset(&key, 0, sizeof(key));
    key.kind = MND_YAML_SCALAR;
    key.as.scalar.style = MND_YAML_PLAIN;

    while (ok && (start < path_len) && (current.count > 0))
    {
        end = start;
        while ((end < path_len) && (path[end] != '.'))
        {
            ++end;
        }
        key.as.scalar.value = path + start;
        key.as.scalar.value_len = end - start;
        next.count = 0;
        for (i = 0; ok && (i < current.count); ++i)
        {
            ok = path_visit(allocator, current.items[i], &key, &stack, &next);
        }
        swap = current;
        current = next;
        next = swap;
        start = end + 1;
    }

    /*
     * 最後のセグメントの結果のうち、シーケンスを要素に展開する
     */
    for (i = 0; ok && (i < current.count); ++i)
    {
        ok = path_visit(allocator, current.items[i], NULL, &stack, result);
    }

    path_list_free(allocator, &current);
    path_list_free(allocator, &next);
    path_list_free(allocator, &stack);
    if (!ok)
    {
        path_list_free(allocator, result);
    }

    return ok;
}

bool mnd_yaml_path_find(const mnd_yaml_doc_t *doc, const char *path, size_t path_len,
                        mnd_yaml_matches_t *out, mnd_diag_list_t *diags)
{
    path_list_t result = { NULL, 0, 0 };
    int shown_len;

    out->nodes = NULL;
    out->count = 0;

    if (!path_is_valid(path, path_len))
    {
        if (diags != NULL)
        {
            shown_len = 0;
            if (path != NULL)
            {
                shown_len = (path_len > (size_t)INT_MAX) ? INT_MAX : (int)path_len;
            }
            mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "invalid key path \"%.*s\"",
                         shown_len, (path != NULL) ? path : "");
        }
        return false;
    }
    if (doc == NULL)
    {
        return true;
    }

    if (!path_match(doc, path, path_len, &result))
    {
        if (diags != NULL)
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory");
        }
        return false;
    }
    if (result.count == 0)
    {
        path_list_free(&doc->allocator, &result);
        return true;
    }
    out->nodes = result.items;
    out->count = result.count;

    return true;
}

void mnd_yaml_matches_free(const mnd_yaml_doc_t *doc, mnd_yaml_matches_t *matches)
{
    if (matches == NULL)
    {
        return;
    }
    if ((doc != NULL) && (matches->nodes != NULL))
    {
        doc->allocator.free(doc->allocator.ctx, (void *)matches->nodes);
    }
    matches->nodes = NULL;
    matches->count = 0;
}
