/* BlockParser: コンテナの照合、ブロックの開始、遅延継続行、閉じる処理、範囲の確定 */

#include "markdown/md.h"
#include "markdown/md_block.h"
#include "markdown/md_internal.h"
#include "markdown/md_lines.h"
#include "markdown/md_scan.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 1 行の処理の状態 */
typedef struct block_line
{
    mnd_md_phys_line_t phys;    /* 物理的な行 */
    mnd_md_scan_pos_t cur;      /* コンテナが使ったバイトの後ろ */
    mnd_md_scan_indent_t indent; /* cur からのインデントと最初の空白でない文字 */
} block_line_t;

static size_t open_count(const mnd_md_parser_t *parser)
{
    return parser->open.count;
}

static mnd_md_node_t *open_node(const mnd_md_parser_t *parser, size_t index)
{
    const mnd_md_open_block_t *entry = (const mnd_md_open_block_t *)mnd_md_array_at(&parser->open, index);

    return entry->node;
}

/* 最も内側の開いているブロック */
static mnd_md_node_t *open_tip(const mnd_md_parser_t *parser)
{
    return open_node(parser, open_count(parser) - 1);
}

/* node を開いているブロックの配列の末尾に加える。メモリ不足なら false */
static bool open_push(mnd_md_parser_t *parser, mnd_md_node_t *node)
{
    mnd_md_open_block_t *entry = (mnd_md_open_block_t *)mnd_md_array_push(&parser->open);

    if (entry == NULL)
    {
        return false;
    }
    entry->node = node;

    return true;
}

/* 行を受け付けるリーフか（開いているリーフの行は leaf_lines に記録する） */
static bool is_leaf(const mnd_md_node_t *node)
{
    return node->kind == MND_MD_PARAGRAPH;
}

/* cur から行の残りのインデントを求め、line->indent に入れる */
static void line_scan_indent(const mnd_md_parser_t *parser, block_line_t *line)
{
    mnd_md_scan_indent(parser->data, line->phys.line_end, &line->cur, &line->indent);
}

/*
 * 開いているリーフに行を 1 つ記録する。prefix は物理的な行の先頭から content_start まで、
 * content は content_start から行末記号の直前まで。メモリ不足なら false
 */
static bool leaf_add_line(mnd_md_parser_t *parser, const block_line_t *line, size_t content_start,
                          size_t pad, bool lazy)
{
    mnd_md_line_t *rec = (mnd_md_line_t *)mnd_md_array_push(&parser->leaf_lines);

    if (rec == NULL)
    {
        return false;
    }
    rec->line = mnd_md_parser_line_of(parser, content_start);
    rec->prefix.start = line->phys.line_start;
    rec->prefix.end = content_start;
    rec->content.start = content_start;
    rec->content.end = line->phys.line_end;
    rec->pad = (unsigned char)pad;
    rec->eol_len = (unsigned char)line->phys.eol_len;
    rec->lazy = lazy;

    return true;
}

/*
 * 閉じるリーフの行を、作業用の配列から文書のアリーナへ移して node->lines に設定する。
 * 範囲は最初の行の内容の開始から最後の行の内容の終わりまで（リーフの範囲の規則）。メモリ不足なら false
 */
static bool leaf_finalize(mnd_md_parser_t *parser, mnd_md_node_t *node)
{
    const mnd_md_line_t *first = NULL;
    const mnd_md_line_t *last = NULL;
    mnd_md_line_t *lines = NULL;
    size_t count = parser->leaf_lines.count;

    if (count > 0)
    {
        lines = (mnd_md_line_t *)mnd_md_arena_alloc_array(&parser->doc->arena, count, sizeof(*lines));
        if (lines == NULL)
        {
            return false;
        }
        memcpy(lines, parser->leaf_lines.items, count * sizeof(*lines));
        first = &lines[0];
        last = &lines[count - 1];
        node->span.start = first->content.start;
        node->span.end = last->content.end;
    }
    node->lines = lines;
    node->line_count = count;
    mnd_md_array_clear(&parser->leaf_lines);

    return true;
}

/* 最も内側の開いているブロックを閉じ、範囲と行番号を確定する（根は閉じない）。メモリ不足なら false */
static bool close_tip(mnd_md_parser_t *parser)
{
    mnd_md_node_t *node = open_tip(parser);

    if (is_leaf(node))
    {
        if (!leaf_finalize(parser, node))
        {
            return false;
        }
    }
    mnd_md_parser_set_node_lines(parser, node);
    mnd_md_array_pop(&parser->open);

    return true;
}

/* 開いているブロックが count 個になるまで、内側から閉じる。メモリ不足なら false */
static bool close_to(mnd_md_parser_t *parser, size_t count)
{
    while (open_count(parser) > count)
    {
        if (!close_tip(parser))
        {
            return false;
        }
    }

    return true;
}

/*
 * 開いているブロックを根の次から順に、現在の行と照合する。照合できたブロックの数（根を含む）を返す。
 * 照合できたコンテナが使ったバイトの分だけ line->cur を進め、line->indent を更新する
 */
static size_t match_open_blocks(mnd_md_parser_t *parser, block_line_t *line)
{
    size_t matched = 1;
    size_t i = 0;

    for (i = 1; i < open_count(parser); i++)
    {
        const mnd_md_node_t *node = open_node(parser, i);
        bool ok = false;

        switch (node->kind)
        {
        case MND_MD_PARAGRAPH:
            /* 段落は空行で終わる */
            ok = !line->indent.blank;
            break;
        default:
            ok = false;
            break;
        }
        if (!ok)
        {
            break;
        }
        matched = i + 1;
    }

    return matched;
}

/* 最も内側の開いているブロック（コンテナ）の最後の子として、現在の行から始まる段落を開く。メモリ不足なら false */
static bool open_paragraph(mnd_md_parser_t *parser, const block_line_t *line)
{
    mnd_md_node_t *node = NULL;
    mnd_span_t span;

    span.start = line->indent.pos;
    span.end = line->indent.pos;
    node = mnd_md_node_new(parser->doc, MND_MD_PARAGRAPH, span);
    if (node == NULL)
    {
        return false;
    }
    mnd_md_node_append_child(open_tip(parser), node);

    return open_push(parser, node);
}

/* 物理的な行を 1 つ処理する（ブロックの段階の 1 行の処理）。メモリ不足なら false */
static bool process_line(mnd_md_parser_t *parser, const mnd_md_phys_line_t *phys)
{
    block_line_t line;
    size_t matched = 0;

    line.phys = *phys;
    line.cur.pos = phys->line_start;
    line.cur.column = 0;
    line.cur.pad = 0;
    line_scan_indent(parser, &line);

    /* 開いているブロックの照合（設計の「ブロックの段階」の 2） */
    matched = match_open_blocks(parser, &line);

    /*
     * 新しいブロックの開始（3）と遅延継続行（4）は、コンテナとリーフの規則とともに加える。
     * 段落だけを扱う間は、すべて照合でき最も内側が段落なら、行をその段落に加える（段落の行は先頭の空白を除く）
     */
    if ((matched == open_count(parser)) && (open_tip(parser)->kind == MND_MD_PARAGRAPH))
    {
        return leaf_add_line(parser, &line, line.indent.pos, 0, false);
    }

    /* 5: 照合できなかったブロックを閉じる。空行は段落を作らず、それ以外は新しい段落を開いて行を加える */
    if (!close_to(parser, matched))
    {
        return false;
    }
    if (line.indent.blank)
    {
        return true;
    }
    if (!open_paragraph(parser, &line))
    {
        return false;
    }

    return leaf_add_line(parser, &line, line.indent.pos, 0, false);
}

bool mnd_md_block_run(mnd_md_parser_t *parser)
{
    mnd_md_node_t *root = parser->doc->root;
    mnd_md_phys_line_t phys;
    size_t pos = parser->body_start;

    if (!open_push(parser, root))
    {
        return false;
    }
    while (mnd_md_lines_next(parser->data, parser->len, &pos, &phys))
    {
        if (!process_line(parser, &phys))
        {
            return false;
        }
    }

    /* 文書の終わり: 根を除くすべてのブロックを閉じ、根の範囲を [body_start, len) にする */
    if (!close_to(parser, 1))
    {
        return false;
    }
    root->span.start = parser->body_start;
    root->span.end = parser->len;
    mnd_md_parser_set_node_lines(parser, root);
    mnd_md_array_pop(&parser->open);

    return true;
}
