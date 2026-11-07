/* tree_check: 構文木の範囲と行番号の不変条件の検査 */

#include "tree_check.h"

#include <naos/naos.h>

#include "foundation/source.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * 行番号の表: data[0..len) の LF の位置を昇順に持つ。offset の行番号は、offset より前にある LF の数 + 1
 * （mnd_source_line_of と同じ数え方。CRLF も LF で数え、単独の CR は数えない）
 */
typedef struct lf_table
{
    size_t *positions;  /* count が 0 なら NULL */
    size_t count;
} lf_table_t;

/* 検査を待つノードの作業用のスタック */
typedef struct node_stack
{
    const mnd_yaml_node_t **items;
    size_t count;
    size_t cap;
} node_stack_t;

static const char *kind_name(mnd_yaml_kind_t kind)
{
    switch (kind)
    {
    case MND_YAML_SCALAR:
        return "scalar";
    case MND_YAML_MAPPING:
        return "mapping";
    case MND_YAML_SEQUENCE:
        return "sequence";
    default:
        return "unknown";
    }
}

static void set_failure(mnd_yaml_tree_check_result_t *result, const mnd_yaml_node_t *node,
                        const char *format, ...)
{
    va_list args;
    int used = 0;

    if (result == NULL)
    {
        return;
    }
    result->ok = false;
    result->node = node;
    result->message[0] = '\0';
    if (node != NULL)
    {
        used = snprintf(result->message, sizeof(result->message), "%s node span [%lu, %lu) lines %lu-%lu: ",
                        kind_name(node->kind), (unsigned long)node->span.start, (unsigned long)node->span.end,
                        (unsigned long)node->start_line, (unsigned long)node->end_line);
    }
    if ((used < 0) || ((size_t)used >= sizeof(result->message)))
    {
        return;
    }
    va_start(args, format);
    (void)vsnprintf(result->message + used, sizeof(result->message) - (size_t)used, format, args);
    va_end(args);
}

static bool lf_table_build(lf_table_t *table, const char *data, size_t len)
{
    size_t count = 0;
    size_t i;

    table->positions = NULL;
    table->count = 0;
    for (i = 0; i < len; ++i)
    {
        if (data[i] == '\n')
        {
            ++count;
        }
    }
    if (count == 0)
    {
        return true;
    }
    table->positions = (size_t *)malloc(count * sizeof(table->positions[0]));
    if (table->positions == NULL)
    {
        return false;
    }
    for (i = 0; i < len; ++i)
    {
        if (data[i] == '\n')
        {
            table->positions[table->count] = i;
            table->count++;
        }
    }

    return true;
}

/* offset（offset <= len）の行番号 */
static size_t lf_table_line_of(const lf_table_t *table, size_t offset)
{
    size_t lo = 0;
    size_t hi = table->count;

    /* positions[i] < offset を満たす i の数を数える */
    while (lo < hi)
    {
        size_t mid = lo + ((hi - lo) / 2);

        if (table->positions[mid] < offset)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }

    return lo + 1;
}

static bool stack_push(node_stack_t *stack, const mnd_yaml_node_t *node)
{
    if (stack->count == stack->cap)
    {
        size_t new_cap = (stack->cap == 0) ? 16 : (stack->cap * 2);
        const mnd_yaml_node_t **items = NULL;

        items = (const mnd_yaml_node_t **)realloc((void *)stack->items, new_cap * sizeof(stack->items[0]));
        if (items == NULL)
        {
            return false;
        }
        stack->items = items;
        stack->cap = new_cap;
    }
    stack->items[stack->count] = node;
    stack->count++;

    return true;
}

static bool span_contains(mnd_span_t outer, mnd_span_t inner)
{
    return (outer.start <= inner.start) && (inner.end <= outer.end);
}

/* ノード自身の範囲と行番号（不変条件 1 と 4）を確かめる */
static bool check_node(const mnd_yaml_node_t *node, mnd_span_t content, const lf_table_t *table,
                       mnd_yaml_tree_check_result_t *result)
{
    size_t start_line;
    size_t end_line;

    if (node->span.start > node->span.end)
    {
        set_failure(result, node, "span start is greater than end");

        return false;
    }
    if (!span_contains(content, node->span))
    {
        set_failure(result, node, "span is outside the content [%lu, %lu)", (unsigned long)content.start,
                    (unsigned long)content.end);

        return false;
    }
    start_line = lf_table_line_of(table, node->span.start);
    end_line = lf_table_line_of(table, node->span.end);
    if (node->start_line != start_line)
    {
        set_failure(result, node, "start_line is %lu, expected %lu", (unsigned long)node->start_line,
                    (unsigned long)start_line);

        return false;
    }
    if (node->end_line != end_line)
    {
        set_failure(result, node, "end_line is %lu, expected %lu", (unsigned long)node->end_line,
                    (unsigned long)end_line);

        return false;
    }

    return true;
}

/*
 * 親 parent の子 child（原文の順で直前の兄弟は prev。最初の子なら NULL）の連結と位置（不変条件 2 と 3）を
 * 確かめ、ノード自身も確かめる
 */
static bool check_child(const mnd_yaml_node_t *parent, const mnd_yaml_node_t *child, const mnd_yaml_node_t *prev,
                        mnd_span_t content, const lf_table_t *table, mnd_yaml_tree_check_result_t *result)
{
    if (child->parent != parent)
    {
        set_failure(result, child, "parent does not point to the containing %s", kind_name(parent->kind));

        return false;
    }
    if (!check_node(child, content, table, result))
    {
        return false;
    }
    if (!span_contains(parent->span, child->span))
    {
        set_failure(result, child, "span is outside the parent span [%lu, %lu)", (unsigned long)parent->span.start,
                    (unsigned long)parent->span.end);

        return false;
    }
    if ((prev != NULL) && (prev->span.end > child->span.start))
    {
        set_failure(result, child, "span overlaps or precedes the previous sibling [%lu, %lu)",
                    (unsigned long)prev->span.start, (unsigned long)prev->span.end);

        return false;
    }

    return true;
}

/* node の子を原文の順に確かめ、スタックに積む。作業用のメモリが確保できなければ *oom を true にする */
static bool check_children(const mnd_yaml_node_t *node, mnd_span_t content, const lf_table_t *table,
                           node_stack_t *stack, bool *oom, mnd_yaml_tree_check_result_t *result)
{
    const mnd_yaml_node_t *prev = NULL;
    size_t i;

    if (node->kind == MND_YAML_MAPPING)
    {
        if ((node->as.mapping.count > 0) && (node->as.mapping.pairs == NULL))
        {
            set_failure(result, node, "mapping has %lu pairs but pairs is NULL",
                        (unsigned long)node->as.mapping.count);

            return false;
        }
        for (i = 0; i < node->as.mapping.count; ++i)
        {
            const mnd_yaml_pair_t *pair = &node->as.mapping.pairs[i];

            if ((pair->key == NULL) || (pair->value == NULL))
            {
                set_failure(result, node, "pair %lu has a NULL key or value", (unsigned long)i);

                return false;
            }
            if (pair->key->kind != MND_YAML_SCALAR)
            {
                set_failure(result, pair->key, "mapping key is not a scalar");

                return false;
            }
            if (!check_child(node, pair->key, prev, content, table, result)
                || !check_child(node, pair->value, pair->key, content, table, result))
            {
                return false;
            }
            prev = pair->value;
            if (!stack_push(stack, pair->value))
            {
                *oom = true;

                return false;
            }
        }
    }
    else if (node->kind == MND_YAML_SEQUENCE)
    {
        if ((node->as.sequence.count > 0) && (node->as.sequence.items == NULL))
        {
            set_failure(result, node, "sequence has %lu items but items is NULL",
                        (unsigned long)node->as.sequence.count);

            return false;
        }
        for (i = 0; i < node->as.sequence.count; ++i)
        {
            const mnd_yaml_node_t *item = node->as.sequence.items[i];

            if (item == NULL)
            {
                set_failure(result, node, "item %lu is NULL", (unsigned long)i);

                return false;
            }
            if (!check_child(node, item, prev, content, table, result))
            {
                return false;
            }
            prev = item;
            if (!stack_push(stack, item))
            {
                *oom = true;

                return false;
            }
        }
    }
    else if (node->kind != MND_YAML_SCALAR)
    {
        set_failure(result, node, "unknown node kind %d", (int)node->kind);

        return false;
    }

    return true;
}

bool mnd_yaml_tree_check(const mnd_yaml_doc_t *doc, mnd_yaml_tree_check_result_t *result)
{
    const mnd_yaml_node_t *root = NULL;
    const char *data = NULL;
    size_t len = 0;
    mnd_span_t content;
    lf_table_t table;
    node_stack_t stack;
    bool oom = false;
    bool ok = true;

    if (result != NULL)
    {
        result->ok = true;
        result->node = NULL;
        result->message[0] = '\0';
    }
    if (doc == NULL)
    {
        set_failure(result, NULL, "document is NULL");

        return false;
    }
    data = mnd_yaml_doc_data(doc);
    len = mnd_yaml_doc_len(doc);
    content = mnd_yaml_doc_content(doc);
    if (!mnd_span_is_valid(content, len))
    {
        set_failure(result, NULL, "content [%lu, %lu) is not inside the document (len %lu)",
                    (unsigned long)content.start, (unsigned long)content.end, (unsigned long)len);

        return false;
    }
    if ((data == NULL) && (len > 0))
    {
        set_failure(result, NULL, "document data is NULL but len is %lu", (unsigned long)len);

        return false;
    }
    root = mnd_yaml_doc_root(doc);
    if (root == NULL)
    {
        return true;
    }
    if (!lf_table_build(&table, data, len))
    {
        set_failure(result, NULL, "out of memory");

        return false;
    }
    stack.items = NULL;
    stack.count = 0;
    stack.cap = 0;

    if (root->parent != NULL)
    {
        set_failure(result, root, "root has a parent");
        ok = false;
    }
    else if (!check_node(root, content, &table, result))
    {
        ok = false;
    }
    else if (!stack_push(&stack, root))
    {
        oom = true;
        ok = false;
    }

    /* 再帰を使わず、スタックから取り出したノードの子を確かめて積む */
    while (ok && (stack.count > 0))
    {
        const mnd_yaml_node_t *node = stack.items[stack.count - 1];

        stack.count--;
        if (!check_children(node, content, &table, &stack, &oom, result))
        {
            ok = false;
        }
    }
    if (oom)
    {
        set_failure(result, NULL, "out of memory");
    }

    free((void *)stack.items);
    free(table.positions);
    if (ok && (result != NULL))
    {
        result->ok = true;
    }

    return ok;
}

bool mnd_yaml_tree_check_note(const mnd_yaml_doc_t *doc)
{
    mnd_yaml_tree_check_result_t result;

    if (mnd_yaml_tree_check(doc, &result))
    {
        return true;
    }
    NAOS_NOTE("tree_check: %s", result.message);

    return false;
}
