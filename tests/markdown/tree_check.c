/* tree_check: 構文木の範囲の不変条件の検査（再帰を使わない） */

#include "tree_check.h"

#include <naos/naos.h>

#include "foundation/source.h"
#include "markdown/md_lines.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* 根の範囲の開始から内容の終わりまでの物理的な行と、それぞれの行の先頭の行番号 */
typedef struct phys_table
{
    const char *data;
    size_t len;
    size_t base;                /* 根の範囲の開始（本文の開始位置） */
    size_t base_line;           /* base の行番号 */
    mnd_md_phys_line_t *lines;  /* count が 0 なら NULL */
    size_t *line_numbers;       /* lines[i].line_start の行番号 */
    size_t count;
} phys_table_t;

static void set_failure(mnd_md_tree_check_result_t *result, const mnd_md_node_t *node, size_t line_index,
                        const char *format, ...)
{
    va_list args;
    int used = 0;

    result->ok = false;
    result->node = node;
    result->line_index = line_index;
    if (node != NULL)
    {
        used = snprintf(result->message, sizeof(result->message), "node kind %d span [%zu, %zu)",
                        (int)node->kind, node->span.start, node->span.end);
        if ((used >= 0) && (line_index != MND_MD_TREE_CHECK_NO_LINE)
            && ((size_t)used < sizeof(result->message)))
        {
            used += snprintf(result->message + used, sizeof(result->message) - (size_t)used,
                             " line[%zu]", line_index);
        }
        if ((used >= 0) && ((size_t)used < sizeof(result->message)))
        {
            used += snprintf(result->message + used, sizeof(result->message) - (size_t)used, ": ");
        }
    }
    if ((used < 0) || ((size_t)used >= sizeof(result->message)))
    {
        return;
    }
    va_start(args, format);
    (void)vsnprintf(result->message + used, sizeof(result->message) - (size_t)used, format, args);
    va_end(args);
}

static void phys_table_free(phys_table_t *table)
{
    free(table->lines);
    free(table->line_numbers);
    table->lines = NULL;
    table->line_numbers = NULL;
    table->count = 0;
}

/*
 * data[base..len) を mnd_md_lines_next で物理的な行に分け、各行の先頭の行番号を mnd_source_line_of で求める。
 * 行番号は前の行の先頭からの差分で数え、全体で O(len) にする。メモリ不足なら false
 */
static bool phys_table_build(phys_table_t *table, const char *data, size_t len, size_t base)
{
    mnd_md_phys_line_t line;
    size_t pos = base;
    size_t count = 0;
    size_t i;
    size_t delta;

    table->data = data;
    table->len = len;
    table->base = base;
    table->lines = NULL;
    table->line_numbers = NULL;
    table->count = 0;
    if (!mnd_source_line_of(data, len, base, &table->base_line))
    {
        return false;
    }

    while (mnd_md_lines_next(data, len, &pos, &line))
    {
        ++count;
    }
    if (count == 0)
    {
        return true;
    }
    table->lines = malloc(count * sizeof(table->lines[0]));
    table->line_numbers = malloc(count * sizeof(table->line_numbers[0]));
    if ((table->lines == NULL) || (table->line_numbers == NULL))
    {
        phys_table_free(table);

        return false;
    }

    pos = base;
    for (i = 0; i < count; ++i)
    {
        (void)mnd_md_lines_next(data, len, &pos, &table->lines[i]);
        if (i == 0)
        {
            table->line_numbers[i] = table->base_line;
        }
        else
        {
            size_t prev = table->lines[i - 1].line_start;

            (void)mnd_source_line_of(data + prev, len - prev, table->lines[i].line_start - prev, &delta);
            table->line_numbers[i] = table->line_numbers[i - 1] + (delta - 1);
        }
    }
    table->count = count;

    return true;
}

/* line_start <= offset の最後の物理的な行の番号。そのような行がなければ false */
static bool phys_table_find(const phys_table_t *table, size_t offset, size_t *index)
{
    size_t lo = 0;
    size_t hi = table->count;

    /* [lo, hi) の中で line_start <= offset を満たす最後の行を探す */
    while (lo < hi)
    {
        size_t mid = lo + ((hi - lo) / 2);

        if (table->lines[mid].line_start <= offset)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    if (lo == 0)
    {
        return false;
    }
    *index = lo - 1;

    return true;
}

/*
 * offset（base <= offset <= len）の行番号。mnd_source_line_of と同じ数え方。
 * 物理的な行の内容 [line_start, line_end] には LF がないので、その行の先頭の行番号と同じ。
 * 行末記号の中や後ろでは、line_end から先を mnd_source_line_of で数える
 */
static size_t phys_table_line_of(const phys_table_t *table, size_t offset)
{
    size_t index;
    size_t from;
    size_t base_line;
    size_t delta = 1;

    if (phys_table_find(table, offset, &index))
    {
        if (offset <= table->lines[index].line_end)
        {
            return table->line_numbers[index];
        }
        from = table->lines[index].line_end;
        base_line = table->line_numbers[index];
    }
    else
    {
        from = table->base;
        base_line = table->base_line;
    }
    if (offset > from)
    {
        (void)mnd_source_line_of(table->data + from, table->len - from, offset - from, &delta);
    }

    return base_line + (delta - 1);
}

static bool span_contains(mnd_span_t outer, mnd_span_t inner)
{
    return (outer.start <= inner.start) && (inner.end <= outer.end);
}

/* ノードの連結と範囲（1〜4）を確かめる */
static bool check_structure(const mnd_md_node_t *node, const mnd_md_node_t *root, size_t len,
                            mnd_md_tree_check_result_t *result)
{
    const mnd_md_node_t *parent = node->parent;

    if (!mnd_span_is_valid(node->span, len))
    {
        set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE,
                    "span is reversed or outside the content (len %zu)", len);

        return false;
    }
    if (node == root)
    {
        if ((parent != NULL) || (node->prev != NULL) || (node->next != NULL))
        {
            set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE, "root has a parent or siblings");

            return false;
        }
        if (node->kind != MND_MD_DOCUMENT)
        {
            set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE, "root is not MND_MD_DOCUMENT");

            return false;
        }
        if (node->span.end != len)
        {
            set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE,
                        "root span does not end at the content length %zu", len);

            return false;
        }

        return true;
    }

    if (parent == NULL)
    {
        set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE, "non-root node has no parent");

        return false;
    }
    if (((node->prev == NULL) && (parent->first_child != node))
        || ((node->prev != NULL) && ((node->prev->next != node) || (node->prev->parent != parent)))
        || ((node->next == NULL) && (parent->last_child != node))
        || ((node->next != NULL) && ((node->next->prev != node) || (node->next->parent != parent))))
    {
        set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE,
                    "parent and sibling links are inconsistent");

        return false;
    }
    if (!span_contains(parent->span, node->span))
    {
        set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE, "span is outside the parent span [%zu, %zu)",
                    parent->span.start, parent->span.end);

        return false;
    }
    if ((node->prev != NULL) && (node->prev->span.end > node->span.start))
    {
        set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE,
                    "span overlaps or precedes the previous sibling [%zu, %zu)",
                    node->prev->span.start, node->prev->span.end);

        return false;
    }

    return true;
}

/* start_line と end_line（5）を確かめる */
static bool check_line_numbers(const mnd_md_node_t *node, const phys_table_t *table,
                               mnd_md_tree_check_result_t *result)
{
    size_t last = (node->span.start == node->span.end) ? node->span.start : (node->span.end - 1);
    size_t start_line = phys_table_line_of(table, node->span.start);
    size_t end_line = phys_table_line_of(table, last);

    if (node->start_line != start_line)
    {
        set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE, "start_line is %zu, expected %zu",
                    node->start_line, start_line);

        return false;
    }
    if (node->end_line != end_line)
    {
        set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE, "end_line is %zu, expected %zu",
                    node->end_line, end_line);

        return false;
    }

    return true;
}

/* 1 つの行（6）を確かめる。prev は直前の行（最初の行なら NULL） */
static bool check_line(const mnd_md_node_t *node, size_t index, const mnd_md_line_t *prev,
                       const phys_table_t *table, mnd_md_tree_check_result_t *result)
{
    const mnd_md_line_t *line = &node->lines[index];
    const mnd_md_phys_line_t *phys;
    size_t phys_index;

    if ((line->prefix.start > line->prefix.end) || (line->prefix.end != line->content.start)
        || (line->content.start > line->content.end))
    {
        set_failure(result, node, index,
                    "prefix [%zu, %zu) and content [%zu, %zu) are not adjacent and ordered",
                    line->prefix.start, line->prefix.end, line->content.start, line->content.end);

        return false;
    }
    if ((line->prefix.start < table->base) || (line->content.end > table->len)
        || !phys_table_find(table, line->content.start, &phys_index)
        || (line->content.start > table->lines[phys_index].line_end))
    {
        set_failure(result, node, index, "content start %zu is not inside a physical line",
                    line->content.start);

        return false;
    }
    phys = &table->lines[phys_index];
    if (line->prefix.start != phys->line_start)
    {
        set_failure(result, node, index, "prefix starts at %zu, not at the physical line start %zu",
                    line->prefix.start, phys->line_start);

        return false;
    }
    if (line->content.end > phys->line_end)
    {
        set_failure(result, node, index,
                    "content [%zu, %zu) goes past the physical line end %zu (contains a line terminator)",
                    line->content.start, line->content.end, phys->line_end);

        return false;
    }
    if (line->eol_len != phys->eol_len)
    {
        set_failure(result, node, index, "eol_len is %u, expected %zu", (unsigned)line->eol_len,
                    phys->eol_len);

        return false;
    }
    if (line->pad > 3)
    {
        set_failure(result, node, index, "pad %u is greater than 3", (unsigned)line->pad);

        return false;
    }
    if (line->line != table->line_numbers[phys_index])
    {
        set_failure(result, node, index, "line is %zu, expected %zu", line->line,
                    table->line_numbers[phys_index]);

        return false;
    }
    if (!span_contains(node->span, line->content))
    {
        set_failure(result, node, index, "content [%zu, %zu) is outside the node span",
                    line->content.start, line->content.end);

        return false;
    }
    if ((prev != NULL) && ((line->prefix.start <= prev->prefix.start) || (prev->content.end > line->prefix.start)))
    {
        set_failure(result, node, index,
                    "line does not come after the previous line (prefix start %zu, previous %zu)",
                    line->prefix.start, prev->prefix.start);

        return false;
    }

    return true;
}

static bool check_lines(const mnd_md_node_t *node, const phys_table_t *table,
                        mnd_md_tree_check_result_t *result)
{
    size_t i;

    if (node->line_count == 0)
    {
        return true;
    }
    if (node->lines == NULL)
    {
        set_failure(result, node, MND_MD_TREE_CHECK_NO_LINE, "line_count is %zu but lines is NULL",
                    node->line_count);

        return false;
    }
    for (i = 0; i < node->line_count; ++i)
    {
        if (!check_line(node, i, (i == 0) ? NULL : &node->lines[i - 1], table, result))
        {
            return false;
        }
    }

    return true;
}

bool mnd_md_tree_check(const mnd_md_doc_t *doc, mnd_md_tree_check_result_t *result)
{
    mnd_md_tree_check_result_t local;
    mnd_md_tree_check_result_t *out = (result != NULL) ? result : &local;
    const mnd_md_node_t *root;
    const mnd_md_node_t *node;
    const char *data;
    size_t len;
    phys_table_t table;
    bool ok = true;

    out->ok = true;
    out->node = NULL;
    out->line_index = MND_MD_TREE_CHECK_NO_LINE;
    out->message[0] = '\0';

    root = mnd_md_doc_root(doc);
    if (root == NULL)
    {
        set_failure(out, NULL, MND_MD_TREE_CHECK_NO_LINE, "document or root is NULL");

        return false;
    }
    data = mnd_md_doc_data(doc);
    len = mnd_md_doc_len(doc);

    /* 物理的な行の表の前に、根の範囲を確かめる（表は根の範囲の開始から作る） */
    if (!check_structure(root, root, len, out))
    {
        return false;
    }
    if (!phys_table_build(&table, data, len, root->span.start))
    {
        set_failure(out, NULL, MND_MD_TREE_CHECK_NO_LINE, "out of memory");

        return false;
    }

    for (node = root; node != NULL; node = mnd_md_node_next(node, root))
    {
        if (!check_structure(node, root, len, out) || !check_line_numbers(node, &table, out)
            || !check_lines(node, &table, out))
        {
            ok = false;
            break;
        }
    }
    phys_table_free(&table);

    return ok;
}

bool mnd_md_tree_check_note(const mnd_md_doc_t *doc)
{
    mnd_md_tree_check_result_t result;

    if (mnd_md_tree_check(doc, &result))
    {
        return true;
    }
    NAOS_NOTE("tree_check: %s", result.message);

    return false;
}
