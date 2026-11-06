/* Dialect: mnd_md_commonmark、方言の検査、コンテキスト関数、後処理のあとの木の検査 */

#include "markdown/md.h"
#include "markdown/md_dialect.h"
#include "markdown/md_internal.h"
#include "markdown/md_scan.h"

#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>

const mnd_md_dialect_t mnd_md_commonmark = { "commonmark", NULL, 0, NULL, 0, NULL };

/* 方言の検査 */

const char *mnd_md_dialect_name(const mnd_md_dialect_t *dialect)
{
    return (dialect->name != NULL) ? dialect->name : "";
}

/* インラインの段階の CommonMark の特別な文字（方言のインラインの規則の開始文字にできない）。行末を含む */
static bool is_commonmark_special(unsigned char c)
{
    return ((c == '\n') || (c == '\r') || (c == '\\') || (c == '`') || (c == '&') || (c == '<') || (c == '*')
            || (c == '_') || (c == '[') || (c == ']') || (c == '!'));
}

/* kind が、ブロックの規則の [0, block_count) か、インラインの規則の [0, inline_count) で使われているか */
static bool kind_used(const mnd_md_dialect_t *dialect, mnd_md_kind_t kind, size_t block_count,
                      size_t inline_count)
{
    size_t i = 0;

    for (i = 0; i < block_count; i++)
    {
        if (dialect->block_rules[i].kind == kind)
        {
            return true;
        }
    }

    for (i = 0; i < inline_count; i++)
    {
        if (dialect->inline_rules[i].kind == kind)
        {
            return true;
        }
    }

    return false;
}

/* ブロックの規則 index が正しいか（それより前の規則は正しいとする） */
static bool block_rule_valid(const mnd_md_dialect_t *dialect, size_t index)
{
    const mnd_md_block_rule_t *rule = NULL;

    if (dialect->block_rules == NULL)
    {
        return false;
    }

    rule = &dialect->block_rules[index];

    return ((rule->kind >= MND_MD_KIND_DIALECT) && (rule->try_start != NULL) && (rule->try_continue != NULL)
            && !kind_used(dialect, rule->kind, index, 0));
}

/* インラインの規則 index が正しいか（ブロックの規則と、それより前のインラインの規則は正しいとする） */
static bool inline_rule_valid(const mnd_md_dialect_t *dialect, size_t index)
{
    const mnd_md_inline_rule_t *rule = NULL;

    if (dialect->inline_rules == NULL)
    {
        return false;
    }

    rule = &dialect->inline_rules[index];
    if ((rule->kind < MND_MD_KIND_DIALECT) || is_commonmark_special(rule->trigger))
    {
        return false;
    }

    if (rule->delimiter ? (rule->match_delims == NULL) : (rule->try_match == NULL))
    {
        return false;
    }

    return !kind_used(dialect, rule->kind, dialect->block_rule_count, index);
}

bool mnd_md_dialect_check(const mnd_md_dialect_t *dialect, const char *path, mnd_diag_list_t *diags)
{
    size_t i = 0;

    for (i = 0; i < dialect->block_rule_count; i++)
    {
        if (!block_rule_valid(dialect, i))
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "dialect \"%s\": invalid %s rule %zu",
                         mnd_md_dialect_name(dialect), "block", i);
            return false;
        }
    }

    for (i = 0; i < dialect->inline_rule_count; i++)
    {
        if (!inline_rule_valid(dialect, i))
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "dialect \"%s\": invalid %s rule %zu",
                         mnd_md_dialect_name(dialect), "inline", i);
            return false;
        }
    }

    return true;
}

const mnd_md_block_rule_t *mnd_md_dialect_block_rule(const mnd_md_dialect_t *dialect, mnd_md_kind_t kind,
                                                     size_t *index)
{
    size_t i = 0;

    if (kind < MND_MD_KIND_DIALECT)
    {
        return NULL;
    }

    for (i = 0; i < dialect->block_rule_count; i++)
    {
        if (dialect->block_rules[i].kind == kind)
        {
            if (index != NULL)
            {
                *index = i;
            }

            return &dialect->block_rules[i];
        }
    }

    return NULL;
}

/* ブロックの規則のコンテキスト関数 */

void mnd_md_block_ctx_init(mnd_md_block_ctx_t *ctx, mnd_md_parser_t *parser, mnd_md_block_ctx_mode_t mode,
                           const mnd_md_block_rule_t *rule, size_t line_start, size_t line_end)
{
    ctx->parser = parser;
    ctx->mode = mode;
    ctx->rule = rule;
    ctx->line_start = line_start;
    ctx->line_end = line_end;
    ctx->line_no = mnd_md_parser_line_of(parser, line_start);
    ctx->pos = line_start;
    ctx->column = 0;
    ctx->pad = 0;
    ctx->paragraph = NULL;
    ctx->paragraph_lines = 0;
    ctx->paragraph_matched = false;
    ctx->opened = NULL;
    ctx->take = 0;
    ctx->invalid = false;
    ctx->oom = false;
}

mnd_md_line_view_t mnd_md_block_ctx_line(const mnd_md_block_ctx_t *ctx)
{
    mnd_md_line_view_t view;
    mnd_md_scan_pos_t cur;
    mnd_md_scan_indent_t indent;

    cur.pos = ctx->pos;
    cur.column = ctx->column;
    cur.pad = ctx->pad;
    mnd_md_scan_indent(ctx->parser->data, ctx->line_end, &cur, &indent);

    view.data = ctx->parser->data;
    view.pos = ctx->pos;
    view.end = ctx->line_end;
    view.column = ctx->column;
    view.indent = indent.indent;
    view.first_nonspace = indent.pos;
    view.blank = indent.blank;
    view.line = ctx->line_no;

    return view;
}

void mnd_md_block_ctx_advance(mnd_md_block_ctx_t *ctx, size_t bytes)
{
    const char *data = ctx->parser->data;
    size_t column = 0;
    size_t i = 0;

    if (ctx->mode == MND_MD_BLOCK_CTX_FINALIZE)
    {
        ctx->invalid = true;
        return;
    }

    if (bytes > (ctx->line_end - ctx->pos))
    {
        /* 行末を超えて使おうとした */
        ctx->invalid = true;
        bytes = ctx->line_end - ctx->pos;
    }

    if (bytes == 0)
    {
        return;
    }

    /* 残っている仮想の空白は、バイトを使うと一緒に使われる */
    column = ctx->column + ctx->pad;
    for (i = 0; i < bytes; i++)
    {
        column = mnd_md_scan_next_column(column, data[ctx->pos + i]);
    }

    ctx->pos += bytes;
    ctx->column = column;
    ctx->pad = 0;
}

void mnd_md_block_ctx_advance_columns(mnd_md_block_ctx_t *ctx, size_t columns)
{
    mnd_md_scan_pos_t cur;

    if (ctx->mode == MND_MD_BLOCK_CTX_FINALIZE)
    {
        ctx->invalid = true;
        return;
    }

    cur.pos = ctx->pos;
    cur.column = ctx->column;
    cur.pad = ctx->pad;
    if (mnd_md_scan_skip_columns(ctx->parser->data, ctx->line_end, &cur, columns) < columns)
    {
        /* 空白でない文字か行末に達した */
        ctx->invalid = true;
    }

    ctx->pos = cur.pos;
    ctx->column = cur.column;
    ctx->pad = cur.pad;
}

mnd_md_node_t *mnd_md_block_ctx_open(mnd_md_block_ctx_t *ctx, mnd_md_kind_t kind)
{
    mnd_md_node_t *node = NULL;
    mnd_span_t span;

    /* 開けるのは try_start の中で 1 回だけ、規則の種類だけ */
    if ((ctx->mode != MND_MD_BLOCK_CTX_START) || (ctx->opened != NULL) || (kind != ctx->rule->kind))
    {
        ctx->invalid = true;
        return NULL;
    }

    span.start = ctx->pos;
    span.end = ctx->pos;
    node = mnd_md_node_new(ctx->parser->doc, kind, span);
    if (node == NULL)
    {
        ctx->oom = true;
        return NULL;
    }

    ctx->opened = node;

    return node;
}

mnd_md_node_t *mnd_md_block_ctx_paragraph(const mnd_md_block_ctx_t *ctx)
{
    return (ctx->mode == MND_MD_BLOCK_CTX_START) ? ctx->paragraph : NULL;
}

bool mnd_md_block_ctx_take_paragraph_lines(mnd_md_block_ctx_t *ctx, size_t count)
{
    /* 引き取れるのは、同じコンテナの中で続く段落の行を、リーフの規則が引き取るときだけ */
    if ((ctx->mode != MND_MD_BLOCK_CTX_START) || (ctx->paragraph == NULL) || !ctx->paragraph_matched
        || ctx->rule->container || (count == 0) || (count > ctx->paragraph_lines))
    {
        return false;
    }

    ctx->take = count;

    return true;
}

void *mnd_md_block_ctx_alloc(mnd_md_block_ctx_t *ctx, size_t size)
{
    void *ptr = mnd_md_arena_alloc(&ctx->parser->doc->arena, size);

    if (ptr == NULL)
    {
        ctx->oom = true;
    }

    return ptr;
}
