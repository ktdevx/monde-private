/* BlockParser: コンテナの照合、コンテナとリーフの開始、遅延継続行、閉じる処理、範囲の確定 */

#include "markdown/md.h"
#include "markdown/md_block.h"
#include "markdown/md_html.h"
#include "markdown/md_internal.h"
#include "markdown/md_lines.h"
#include "markdown/md_scan.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* インデントがこの桁数以上の行は、ブロックの開始の記号を持たない（インデントコードブロックの桁数） */
#define BLOCK_CODE_INDENT 4

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

static mnd_md_open_block_t *open_entry(const mnd_md_parser_t *parser, size_t index)
{
    return (mnd_md_open_block_t *)mnd_md_array_at(&parser->open, index);
}

static mnd_md_node_t *open_node(const mnd_md_parser_t *parser, size_t index)
{
    return open_entry(parser, index)->node;
}

/* 最も内側の開いているブロックの要素 */
static mnd_md_open_block_t *open_tip_entry(const mnd_md_parser_t *parser)
{
    return open_entry(parser, open_count(parser) - 1);
}

/* 最も内側の開いているブロック */
static mnd_md_node_t *open_tip(const mnd_md_parser_t *parser)
{
    return open_tip_entry(parser)->node;
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

/* 空白かタブ（CommonMark の space or tab） */
static bool is_space_or_tab(char c)
{
    return ((c == ' ') || (c == '\t'));
}

/* リーフか（閉じるときに leaf_finalize で行と範囲を確定する。開いているリーフの行は leaf_lines に記録する） */
static bool is_leaf(const mnd_md_node_t *node)
{
    switch (node->kind)
    {
    case MND_MD_PARAGRAPH:
    case MND_MD_HEADING:
    case MND_MD_THEMATIC_BREAK:
    case MND_MD_CODE_BLOCK:
    case MND_MD_HTML_BLOCK:
        return true;
    default:
        return false;
    }
}

/*
 * 行をそのまま受け付けるリーフか（コードブロック、HTML ブロック）。
 * 最後に照合したブロックがこれらなら、新しいブロックの開始を試さない
 */
static bool accepts_lines(const mnd_md_node_t *node)
{
    return ((node->kind == MND_MD_CODE_BLOCK) || (node->kind == MND_MD_HTML_BLOCK));
}

/*
 * parent の種類のブロックが、child の種類のブロックを子に持てるか。
 * 文書・引用・リスト項目はリスト項目以外（リーフを含む）を、リストはリスト項目だけを持ち、
 * リーフ（段落、見出し、テーマ区切り、コードブロック、HTML ブロック）は子を持たない
 */
static bool can_contain(mnd_md_kind_t parent, mnd_md_kind_t child)
{
    switch (parent)
    {
    case MND_MD_DOCUMENT:
    case MND_MD_BLOCK_QUOTE:
    case MND_MD_ITEM:
        return child != MND_MD_ITEM;
    case MND_MD_LIST:
        return child == MND_MD_ITEM;
    default:
        return false;
    }
}

/*
 * ブロックが空行で終わるか（タイトとルーズの判定）。自身の最後の行が空行か、
 * リストとリスト項目では最後の子が空行で終わる場合
 */
static bool ends_with_blank(const mnd_md_open_block_t *entry)
{
    mnd_md_kind_t kind = entry->node->kind;

    if (entry->last_line_blank)
    {
        return true;
    }

    return (((kind == MND_MD_LIST) || (kind == MND_MD_ITEM)) && entry->last_child_blank);
}

/* cur から行の残りのインデントを求め、line->indent に入れる */
static void line_scan_indent(const mnd_md_parser_t *parser, block_line_t *line)
{
    mnd_md_scan_indent(parser->data, line->phys.line_end, &line->cur, &line->indent);
}

/* cur を最初の空白でない文字（空行では行末）へ進める。残りのタブの仮想の空白も使う */
static void line_advance_to_nonspace(const mnd_md_parser_t *parser, block_line_t *line)
{
    line->cur.pos = line->indent.pos;
    line->cur.column = line->indent.column;
    line->cur.pad = 0;
    line_scan_indent(parser, line);
}

/* cur から columns 桁分の空白を使い（タブの一部なら pad に残す）、line->indent を更新する */
static void line_skip_columns(const mnd_md_parser_t *parser, block_line_t *line, size_t columns)
{
    (void)mnd_md_scan_skip_columns(parser->data, line->phys.line_end, &line->cur, columns);
    line_scan_indent(parser, line);
}

/* 物理的な行の、空白でない最後のバイトの後ろ（空白だけの行では行の先頭） */
static size_t line_trimmed_end(const mnd_md_parser_t *parser, const block_line_t *line)
{
    size_t end = line->phys.line_end;

    while ((end > line->phys.line_start) && is_space_or_tab(parser->data[end - 1]))
    {
        end--;
    }

    return end;
}

/*
 * 開いているリーフに行を 1 つ記録する。prefix は物理的な行の先頭から content_start まで、
 * content は content_start から content_end まで（ATX 見出し以外は行末記号の直前）。メモリ不足なら false
 */
static bool leaf_add_line(mnd_md_parser_t *parser, const block_line_t *line, size_t content_start,
                          size_t content_end, size_t pad, bool lazy)
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
    rec->content.end = content_end;
    rec->pad = (unsigned char)pad;
    rec->eol_len = (unsigned char)line->phys.eol_len;
    rec->lazy = lazy;

    return true;
}

/* 行の内容が空白とタブだけ（空を含む）か */
static bool line_content_is_blank(const mnd_md_parser_t *parser, const mnd_md_line_t *line)
{
    size_t i = 0;

    for (i = line->content.start; i < line->content.end; i++)
    {
        if (!is_space_or_tab(parser->data[i]))
        {
            return false;
        }
    }

    return true;
}

/*
 * 閉じるリーフの行を、作業用の配列から文書のアリーナへ移して node->lines に設定し、範囲を確定する
 * （範囲の規則）。インデントコードブロックは末尾の空行を行に含めない。段落・インデントコード・
 * HTML ブロックの範囲は、最初の行の内容の開始から最後の行の内容の終わりまで。閉じフェンスのない
 * フェンスコードブロックは、最後の行の内容の終わりまで（行がなければ開きフェンスの行の終わりのまま）。
 * 見出し・テーマ区切り・閉じたフェンスコードブロックの範囲は、開始のときに確定している。
 * メモリ不足なら false
 */
static bool leaf_finalize(mnd_md_parser_t *parser, mnd_md_node_t *node)
{
    const mnd_md_line_t *items = (const mnd_md_line_t *)parser->leaf_lines.items;
    const mnd_md_line_t *first = NULL;
    const mnd_md_line_t *last = NULL;
    mnd_md_line_t *lines = NULL;
    size_t count = parser->leaf_lines.count;
    bool fenced = ((node->kind == MND_MD_CODE_BLOCK) && node->as.code_block.fenced);

    if ((node->kind == MND_MD_CODE_BLOCK) && !fenced)
    {
        while ((count > 0) && line_content_is_blank(parser, &items[count - 1]))
        {
            count--;
        }
    }
    if (count > 0)
    {
        lines = (mnd_md_line_t *)mnd_md_arena_alloc_array(&parser->doc->arena, count, sizeof(*lines));
        if (lines == NULL)
        {
            return false;
        }
        memcpy(lines, items, count * sizeof(*lines));
        first = &lines[0];
        last = &lines[count - 1];
        switch (node->kind)
        {
        case MND_MD_PARAGRAPH:
        case MND_MD_HTML_BLOCK:
            node->span.start = first->content.start;
            node->span.end = last->content.end;
            break;
        case MND_MD_CODE_BLOCK:
            if (!fenced)
            {
                node->span.start = first->content.start;
                node->span.end = last->content.end;
            }
            else if (!node->as.code_block.closed)
            {
                node->span.end = last->content.end;
            }
            break;
        default:
            break;
        }
    }
    node->lines = lines;
    node->line_count = count;
    mnd_md_array_clear(&parser->leaf_lines);

    return true;
}

/*
 * 閉じるコンテナの範囲の終わりを確定する（範囲の規則）。リストは最後の項目の終わり。
 * 引用とリスト項目は、最後の子の終わりと、自身の記号が現れた最後の行の空白でない最後のバイトの後ろの
 * 大きい方（末尾の空行は含めない。子のない空のリスト項目はリスト記号の終わり）
 */
static void container_finalize(mnd_md_node_t *node, const mnd_md_open_block_t *entry)
{
    size_t end = entry->marker_end;

    if (node->kind == MND_MD_LIST)
    {
        end = (node->last_child != NULL) ? node->last_child->span.end : node->span.start;
    }
    else if ((node->last_child != NULL) && (node->last_child->span.end > end))
    {
        end = node->last_child->span.end;
    }
    node->span.end = end;
}

/* 最も内側の開いているブロックを閉じ、範囲と行番号を確定する（根は閉じない）。メモリ不足なら false */
static bool close_tip(mnd_md_parser_t *parser)
{
    mnd_md_open_block_t closed = *open_tip_entry(parser);
    mnd_md_node_t *node = closed.node;

    if (is_leaf(node))
    {
        if (!leaf_finalize(parser, node))
        {
            return false;
        }
    }
    else
    {
        container_finalize(node, &closed);
    }
    mnd_md_parser_set_node_lines(parser, node);
    mnd_md_array_pop(&parser->open);
    open_tip_entry(parser)->last_child_blank = ends_with_blank(&closed);

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
 * kind の種類の新しいブロックを span の範囲で作り、最も内側の開いているブロックの最後の子として開く。
 * 最も内側のブロックがその種類の子を持てなければ、持てるブロックまで閉じる。
 * 直前の兄弟が空行で終わっていれば、親（またはリスト項目の親）のリストをルーズにする。
 * 新しいブロックの要素を返す（次に開いているブロックの配列を変更するまで有効）。メモリ不足なら NULL
 */
static mnd_md_open_block_t *add_block(mnd_md_parser_t *parser, mnd_md_kind_t kind, mnd_span_t span)
{
    mnd_md_open_block_t *parent_entry = NULL;
    mnd_md_node_t *parent = NULL;
    mnd_md_node_t *node = NULL;

    while ((open_count(parser) > 1) && !can_contain(open_tip(parser)->kind, kind))
    {
        if (!close_tip(parser))
        {
            return NULL;
        }
    }
    node = mnd_md_node_new(parser->doc, kind, span);
    if (node == NULL)
    {
        return NULL;
    }
    parent_entry = open_tip_entry(parser);
    parent = parent_entry->node;
    if ((parent->last_child != NULL) && parent_entry->last_child_blank)
    {
        if (parent->kind == MND_MD_LIST)
        {
            parent->as.list.tight = false;
        }
        else if ((parent->kind == MND_MD_ITEM) && (parent->parent != NULL)
                 && (parent->parent->kind == MND_MD_LIST))
        {
            parent->parent->as.list.tight = false;
        }
    }
    parent_entry->last_child_blank = false;
    mnd_md_node_append_child(parent, node);
    if (!open_push(parser, node))
    {
        return NULL;
    }

    return open_tip_entry(parser);
}

/* 行の残りが引用の記号で始まるか（インデント 3 以下の `>`） */
static bool line_starts_block_quote(const mnd_md_parser_t *parser, const block_line_t *line)
{
    return (!line->indent.blank && (line->indent.indent < BLOCK_CODE_INDENT)
            && (parser->data[line->indent.pos] == '>'));
}

/* 引用の記号 `>` と、その後ろの空白 1 桁分（タブなら一部）を使う */
static void line_consume_block_quote_marker(const mnd_md_parser_t *parser, block_line_t *line)
{
    line_advance_to_nonspace(parser, line);
    line->cur.pos++;
    line->cur.column++;
    line_skip_columns(parser, line, 1);
}

/*
 * 開いているフェンスコードブロック node と現在の行を照合する（常に照合できる）。
 * 閉じフェンスなら、閉じたことと範囲の終わり（閉じフェンスの行の終わり）を記録して *closed を真にする。
 * そうでなければ、開きフェンスのインデントの桁数までの空白を使う（4.6）
 */
static void match_fenced_code(const mnd_md_parser_t *parser, block_line_t *line, mnd_md_node_t *node,
                              bool *closed)
{
    const mnd_md_code_block_t *code = &node->as.code_block;

    if (!line->indent.blank && (line->indent.indent < BLOCK_CODE_INDENT)
        && mnd_md_scan_close_fence(parser->data, line->indent.pos, line->phys.line_end,
                                   code->fence_char, code->fence_len))
    {
        node->as.code_block.closed = true;
        node->span.end = line->phys.line_end;
        *closed = true;
        return;
    }
    line_skip_columns(parser, line, code->fence_indent);
}

/*
 * 開いているインデントコードブロックと現在の行を照合する。4 桁以上のインデントなら 4 桁を使い（4.7）、
 * 4 桁に満たない空行なら行の空白をすべて使う。それ以外は照合できない
 */
static bool match_indented_code(const mnd_md_parser_t *parser, block_line_t *line)
{
    if (line->indent.indent >= BLOCK_CODE_INDENT)
    {
        line_skip_columns(parser, line, BLOCK_CODE_INDENT);
        return true;
    }
    if (line->indent.blank)
    {
        line_advance_to_nonspace(parser, line);
        return true;
    }

    return false;
}

/*
 * 開いているブロックを根の次から順に、現在の行と照合する。照合できたブロックの数（根を含む）を返す。
 * 照合できたコンテナが使ったバイトの分だけ line->cur を進め、line->indent を更新する。
 * 行が閉じフェンスなら *fence_closed を真にする（行はそこで使い終わる）
 */
static size_t match_open_blocks(mnd_md_parser_t *parser, block_line_t *line, bool *fence_closed)
{
    size_t matched = 1;
    size_t i = 0;

    for (i = 1; i < open_count(parser); i++)
    {
        mnd_md_open_block_t *entry = open_entry(parser, i);
        const mnd_md_node_t *node = entry->node;
        bool ok = false;

        switch (node->kind)
        {
        case MND_MD_BLOCK_QUOTE:
            /* インデント 3 以下の `>` */
            ok = line_starts_block_quote(parser, line);
            if (ok)
            {
                line_consume_block_quote_marker(parser, line);
                entry->marker_end = line_trimmed_end(parser, line);
            }
            break;
        case MND_MD_LIST:
            /* リストは項目の照合に任せる */
            ok = true;
            break;
        case MND_MD_ITEM:
            if (line->indent.blank)
            {
                /* 空行は、子を持つ項目だけを続ける（空の項目のあとの空行で項目は終わる） */
                ok = (node->first_child != NULL);
                if (ok)
                {
                    line_advance_to_nonspace(parser, line);
                }
            }
            else if (line->indent.indent >= node->as.item.content_indent)
            {
                line_skip_columns(parser, line, node->as.item.content_indent);
                ok = true;
            }
            else
            {
                ok = false;
            }
            break;
        case MND_MD_PARAGRAPH:
            /* 段落は空行で終わる */
            ok = !line->indent.blank;
            break;
        case MND_MD_CODE_BLOCK:
            if (node->as.code_block.fenced)
            {
                match_fenced_code(parser, line, entry->node, fence_closed);
                ok = true;
            }
            else
            {
                ok = match_indented_code(parser, line);
            }
            break;
        case MND_MD_HTML_BLOCK:
            /* 種類 6 と 7 は空行で終わる。種類 1〜5 の終了条件は、行を加えたあとに判定する */
            ok = !(line->indent.blank
                   && ((node->as.html_block.type == 6) || (node->as.html_block.type == 7)));
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

/*
 * 照合できなかったブロックを閉じ、現在の行の `>` から始まる引用を開く。
 * *matched を開いているブロックの数にする。メモリ不足なら false
 */
static bool start_block_quote(mnd_md_parser_t *parser, block_line_t *line, size_t *matched)
{
    mnd_md_open_block_t *entry = NULL;
    mnd_span_t span;

    span.start = line->indent.pos;
    span.end = line->indent.pos + 1;
    if (!close_to(parser, *matched))
    {
        return false;
    }
    entry = add_block(parser, MND_MD_BLOCK_QUOTE, span);
    if (entry == NULL)
    {
        return false;
    }
    line_consume_block_quote_marker(parser, line);
    entry->marker_end = line_trimmed_end(parser, line);
    *matched = open_count(parser);

    return true;
}

/*
 * 照合できなかったブロックを閉じ、リスト記号 marker から始まるリスト項目を開く。
 * 最も内側の開いているブロックが、記号の種類（順序付きか、記号の文字）の同じリストでなければ、
 * 新しいリストを開く。*matched を開いているブロックの数にする。メモリ不足なら false
 */
static bool start_list_item(mnd_md_parser_t *parser, block_line_t *line,
                            const mnd_md_scan_list_marker_t *marker, size_t *matched)
{
    mnd_md_open_block_t *entry = NULL;
    const mnd_md_node_t *tip = NULL;
    size_t width = marker->span.end - marker->span.start;
    size_t after_column = line->indent.column + width;
    size_t padding = mnd_md_scan_list_padding(parser->data, marker->span.end, line->phys.line_end,
                                              after_column);
    size_t content_indent = line->indent.indent + width + padding;
    mnd_span_t span;

    if (!close_to(parser, *matched))
    {
        return false;
    }
    tip = open_tip(parser);
    if ((tip->kind != MND_MD_LIST) || (tip->as.list.ordered != marker->ordered)
        || (tip->as.list.marker != marker->marker))
    {
        span.start = marker->span.start;
        span.end = marker->span.start;
        entry = add_block(parser, MND_MD_LIST, span);
        if (entry == NULL)
        {
            return false;
        }
        entry->node->as.list.ordered = marker->ordered;
        entry->node->as.list.marker = marker->marker;
        entry->node->as.list.start = marker->start;
        entry->node->as.list.tight = true;
    }
    entry = add_block(parser, MND_MD_ITEM, marker->span);
    if (entry == NULL)
    {
        return false;
    }
    entry->node->as.item.marker = marker->span;
    entry->node->as.item.content_indent = content_indent;
    entry->marker_end = line_trimmed_end(parser, line);

    /* 記号と、その後ろの空白（内容のインデントまで。タブの一部なら pad に残す）を使う */
    line->cur.pos = marker->span.end;
    line->cur.column = after_column;
    line->cur.pad = 0;
    line_skip_columns(parser, line, padding);
    *matched = open_count(parser);

    return true;
}

/*
 * 照合できなかったブロックを閉じ、kind の種類のリーフを span の範囲で開く。
 * *matched を開いているブロックの数にする。新しいリーフのノードを返す。メモリ不足なら NULL
 */
static mnd_md_node_t *start_leaf(mnd_md_parser_t *parser, mnd_md_kind_t kind, mnd_span_t span,
                                 size_t *matched)
{
    mnd_md_open_block_t *entry = NULL;

    if (!close_to(parser, *matched))
    {
        return NULL;
    }
    entry = add_block(parser, kind, span);
    if (entry == NULL)
    {
        return NULL;
    }
    *matched = open_count(parser);

    return entry->node;
}

/*
 * インデントコードブロックを開く。コードブロックの 4 桁を使い（タブの一部なら pad に残す。4.5、4.7）、
 * 行は手順 5 で加える。メモリ不足なら false
 */
static bool start_indented_code(mnd_md_parser_t *parser, block_line_t *line, size_t *matched)
{
    mnd_md_node_t *node = NULL;
    mnd_span_t span;

    line_skip_columns(parser, line, BLOCK_CODE_INDENT);
    span.start = line->cur.pos;
    span.end = line->cur.pos;
    node = start_leaf(parser, MND_MD_CODE_BLOCK, span, matched);
    if (node == NULL)
    {
        return false;
    }
    node->as.code_block.fenced = false;

    return true;
}

/*
 * ATX 見出しを開き、見出しの内容を行として加えて閉じる。範囲は最初の `#` から行の終わりまで
 * （3.4、3.5）。メモリ不足なら false
 */
static bool start_atx_heading(mnd_md_parser_t *parser, const block_line_t *line,
                              const mnd_md_heading_t *heading, size_t *matched)
{
    mnd_md_node_t *node = NULL;
    mnd_span_t span;

    span.start = line->indent.pos;
    span.end = line->phys.line_end;
    node = start_leaf(parser, MND_MD_HEADING, span, matched);
    if (node == NULL)
    {
        return false;
    }
    node->as.heading = *heading;
    if (!leaf_add_line(parser, line, heading->content.start, heading->content.end, 0, false))
    {
        return false;
    }

    return close_tip(parser);
}

/*
 * フェンスコードブロックを開く。開きフェンスの行は内容の行に含めない（4.6）。範囲は開きフェンスから
 * 行の終わりまでで、続く行と閉じフェンスで広げる。メモリ不足なら false
 */
static bool start_fenced_code(mnd_md_parser_t *parser, const block_line_t *line,
                              const mnd_md_code_block_t *fence, size_t *matched)
{
    mnd_md_node_t *node = NULL;
    mnd_span_t span;

    span.start = line->indent.pos;
    span.end = line->phys.line_end;
    node = start_leaf(parser, MND_MD_CODE_BLOCK, span, matched);
    if (node == NULL)
    {
        return false;
    }
    node->as.code_block = *fence;
    node->as.code_block.fence_indent = line->indent.indent;

    return true;
}

/* HTML ブロックを開く。行は、コンテナのプレフィックスの直後から手順 5 で加える。メモリ不足なら false */
static bool start_html_block(mnd_md_parser_t *parser, const block_line_t *line, int type,
                             size_t *matched)
{
    mnd_md_node_t *node = NULL;
    mnd_span_t span;

    span.start = line->cur.pos;
    span.end = line->cur.pos;
    node = start_leaf(parser, MND_MD_HTML_BLOCK, span, matched);
    if (node == NULL)
    {
        return false;
    }
    node->as.html_block.type = type;

    return true;
}

/*
 * テーマ区切りを開いて閉じる。範囲は最初の記号から行の終わりまで。行は持たない。メモリ不足なら false
 */
static bool start_thematic_break(mnd_md_parser_t *parser, const block_line_t *line, size_t *matched)
{
    mnd_span_t span;

    span.start = line->indent.pos;
    span.end = line->phys.line_end;
    if (start_leaf(parser, MND_MD_THEMATIC_BREAK, span, matched) == NULL)
    {
        return false;
    }

    return close_tip(parser);
}

/*
 * Setext の下線で見出しに変える前に、開いている段落の先頭からリンク参照定義を取り出す位置
 * （定義の解析は 3.4 でここに加える。段落を閉じるときの取り出しと同じ処理を使う）。
 * 段落に行が残れば *remains を真にする。メモリ不足なら false
 */
static bool paragraph_take_definitions(mnd_md_parser_t *parser, bool *remains)
{
    *remains = (parser->leaf_lines.count > 0);

    return true;
}

/*
 * 最も内側の開いている段落（すべてのブロックを照合できた）を、Setext 見出しに変えて閉じる。
 * 段落の先頭のリンク参照定義を先に取り出し、行が残らなければ変えずに *converted を偽にする。
 * 範囲は最初の行の内容の開始から下線の行の終わりまで、内容は下線を除く行の末尾の空白を除いた範囲
 * （3.4、3.5）。下線の行は行に含めない。メモリ不足なら false
 */
static bool start_setext_heading(mnd_md_parser_t *parser, const block_line_t *line, int level,
                                 bool *converted)
{
    mnd_md_node_t *node = open_tip(parser);
    const mnd_md_line_t *lines = NULL;
    const mnd_md_line_t *last = NULL;
    size_t content_end = 0;
    bool remains = false;

    *converted = false;
    if (!paragraph_take_definitions(parser, &remains))
    {
        return false;
    }
    if (!remains)
    {
        return true;
    }
    lines = (const mnd_md_line_t *)parser->leaf_lines.items;
    last = &lines[parser->leaf_lines.count - 1];
    content_end = last->content.end;
    while ((content_end > last->content.start) && is_space_or_tab(parser->data[content_end - 1]))
    {
        content_end--;
    }
    node->kind = MND_MD_HEADING;
    node->as.heading.level = level;
    node->as.heading.setext = true;
    node->as.heading.content.start = lines[0].content.start;
    node->as.heading.content.end = content_end;
    node->span.start = lines[0].content.start;
    node->span.end = line->phys.line_end;
    *converted = true;

    return close_tip(parser);
}

/*
 * 照合できたブロックの中で、新しいブロックの開始を試す（設計の「ブロックの段階」の 3）。
 * コンテナが開始したら、行の残りで続けて試す。何か開始したら *started を真にし、行を使い終わる
 * リーフ（ATX 見出し、Setext 見出し、テーマ区切り、開きフェンス）なら *done も真にする。
 * *matched は、開始したときに開いているブロックの数になる。メモリ不足なら false
 */
static bool try_block_starts(mnd_md_parser_t *parser, block_line_t *line, size_t *matched,
                             bool *started, bool *done)
{
    mnd_md_scan_list_marker_t marker;
    mnd_md_heading_t heading;
    mnd_md_code_block_t fence;
    mnd_md_html_start_t html;
    int level = 0;

    for (;;)
    {
        const mnd_md_node_t *container = open_node(parser, *matched - 1);
        const char *data = parser->data;
        size_t pos = line->indent.pos;
        size_t end = line->phys.line_end;

        if (accepts_lines(container))
        {
            /* コードブロックと HTML ブロックの中では、ブロックは始まらない */
            break;
        }
        if (line->indent.blank)
        {
            /* 空行からはブロックが始まらない */
            break;
        }
        if (line->indent.indent >= BLOCK_CODE_INDENT)
        {
            /* 最も内側の開いているブロックが段落でなければ（遅延継続行でもなければ）、インデントコード */
            if (open_tip(parser)->kind != MND_MD_PARAGRAPH)
            {
                if (!start_indented_code(parser, line, matched))
                {
                    return false;
                }
                *started = true;
            }
            break;
        }
        /* 5.1: 方言のブロックの規則を登録順に試す */
        if (line_starts_block_quote(parser, line))
        {
            if (!start_block_quote(parser, line, matched))
            {
                return false;
            }
            *started = true;
            continue;
        }
        memset(&heading, 0, sizeof(heading));
        if (mnd_md_scan_atx_heading(data, pos, end, &heading))
        {
            *started = true;
            *done = true;
            return start_atx_heading(parser, line, &heading, matched);
        }
        memset(&fence, 0, sizeof(fence));
        if (mnd_md_scan_open_fence(data, pos, end, &fence))
        {
            *started = true;
            *done = true;
            return start_fenced_code(parser, line, &fence, matched);
        }
        /* 種類 7 は、段落（遅延継続行の候補を含む）を中断しない */
        if (mnd_md_html_block_start(data, pos, end, &html)
            && (html.interrupts_paragraph || (open_tip(parser)->kind != MND_MD_PARAGRAPH)))
        {
            *started = true;
            return start_html_block(parser, line, html.type, matched);
        }
        if (container->kind == MND_MD_PARAGRAPH)
        {
            level = mnd_md_scan_setext_underline(data, pos, end);
            if (level > 0)
            {
                bool converted = false;

                if (!start_setext_heading(parser, line, level, &converted))
                {
                    return false;
                }
                if (converted)
                {
                    *started = true;
                    *done = true;
                    return true;
                }
                /* 段落がすべて定義だったので見出しにならない。ほかのブロックの開始を続けて試す */
                if (*matched > open_count(parser))
                {
                    *matched = open_count(parser);
                }
            }
        }
        if (mnd_md_scan_thematic_break(data, pos, end, NULL))
        {
            *started = true;
            *done = true;
            return start_thematic_break(parser, line, matched);
        }
        if (mnd_md_scan_list_marker(data, pos, end, &marker))
        {
            /* 段落を中断できるのは、空でなく、順序付きなら開始番号が 1 の項目だけ */
            bool interrupts = (!marker.rest_blank && (!marker.ordered || (marker.start == 1UL)));

            if ((open_node(parser, *matched - 1)->kind != MND_MD_PARAGRAPH) || interrupts)
            {
                if (!start_list_item(parser, line, &marker, matched))
                {
                    return false;
                }
                *started = true;
                continue;
            }
        }
        break;
    }

    return true;
}

/*
 * 照合できなかったブロックを閉じたあと、空行を記録する（タイトとルーズの判定）。
 * 空行なら最も内側のブロックの最後の子が空行で終わるとし、最も内側のブロックから根まで、
 * 最後の行が空行かどうかを設定する。引用、フェンスコードブロック、この行で始まった子のないリスト項目は、
 * 空行で終わったとしない
 */
static void record_blank_line(mnd_md_parser_t *parser, const block_line_t *line)
{
    size_t tip_index = open_count(parser) - 1;
    mnd_md_open_block_t *tip_entry = open_entry(parser, tip_index);
    const mnd_md_node_t *node = tip_entry->node;
    bool blank = line->indent.blank;
    bool last_line_blank = blank;
    size_t i = tip_index;

    if (blank && (node->last_child != NULL))
    {
        tip_entry->last_child_blank = true;
    }
    if ((node->kind == MND_MD_BLOCK_QUOTE)
        || ((node->kind == MND_MD_CODE_BLOCK) && node->as.code_block.fenced))
    {
        last_line_blank = false;
    }
    else if ((node->kind == MND_MD_ITEM) && (node->first_child == NULL)
             && (node->span.start >= line->phys.line_start))
    {
        last_line_blank = false;
    }
    for (;;)
    {
        mnd_md_open_block_t *entry = open_entry(parser, i);

        entry->last_line_blank = last_line_blank;
        if (i < tip_index)
        {
            entry->last_child_blank = ends_with_blank(open_entry(parser, i + 1));
        }
        if (i == 0)
        {
            break;
        }
        i--;
    }
}

/*
 * 最も内側の開いているブロックの最後の子として、現在の行から始まる段落を開く
 * （リストの中なら、リストを閉じてから開く）。メモリ不足なら false
 */
static bool open_paragraph(mnd_md_parser_t *parser, const block_line_t *line)
{
    mnd_span_t span;

    span.start = line->indent.pos;
    span.end = line->indent.pos;

    return (add_block(parser, MND_MD_PARAGRAPH, span) != NULL);
}

/*
 * 最も内側の開いているリーフ（コードブロック、HTML ブロック）に、コンテナと自身が使ったバイトの後ろから
 * 行を加える（タブの一部を使ったなら pad を記録する）。HTML ブロックの種類 1〜5 は、行が終了条件を
 * 満たせば閉じる。メモリ不足なら false
 */
static bool add_line_to_code_or_html(mnd_md_parser_t *parser, const block_line_t *line)
{
    const mnd_md_node_t *tip = open_tip(parser);
    int type = 0;

    if (!leaf_add_line(parser, line, line->cur.pos, line->phys.line_end, line->cur.pad, false))
    {
        return false;
    }
    if (tip->kind == MND_MD_HTML_BLOCK)
    {
        type = tip->as.html_block.type;
        if ((type >= 1) && (type <= 5)
            && mnd_md_html_block_end(parser->data, line->cur.pos, line->phys.line_end, type))
        {
            return close_tip(parser);
        }
    }

    return true;
}

/* 物理的な行を 1 つ処理する（ブロックの段階の 1 行の処理）。メモリ不足なら false */
static bool process_line(mnd_md_parser_t *parser, const mnd_md_phys_line_t *phys)
{
    block_line_t line;
    size_t matched = 0;
    size_t count = 0;
    bool tip_is_paragraph = false;
    bool fence_closed = false;
    bool started = false;
    bool done = false;

    line.phys = *phys;
    line.cur.pos = phys->line_start;
    line.cur.column = 0;
    line.cur.pad = 0;
    line_scan_indent(parser, &line);

    /* 2: 開いているブロックの照合 */
    matched = match_open_blocks(parser, &line, &fence_closed);
    if (fence_closed)
    {
        /* 閉じフェンスの行: フェンスコードブロック（最も内側）を閉じ、行を使い終わる */
        if (!close_tip(parser))
        {
            return false;
        }
        record_blank_line(parser, &line);
        return true;
    }
    count = open_count(parser);
    tip_is_paragraph = (open_tip(parser)->kind == MND_MD_PARAGRAPH);

    /* 3: 新しいブロックの開始 */
    if (!try_block_starts(parser, &line, &matched, &started, &done))
    {
        return false;
    }
    if (done)
    {
        /* 行を使い終わるリーフが始まった */
        record_blank_line(parser, &line);
        return true;
    }

    /*
     * 4: 新しいブロックが始まらず、照合できなかったブロックがあり、最も内側が段落で、空行でなければ、
     * 遅延継続行としてその段落に加える（段落の行は先頭の空白を除く）
     */
    if (!started && (matched < count) && !line.indent.blank && tip_is_paragraph)
    {
        return leaf_add_line(parser, &line, line.indent.pos, line.phys.line_end, 0, true);
    }

    /*
     * 5: 照合できなかったブロックを閉じ、空行を記録し、行をリーフ（段落、コードブロック、HTML ブロック）に
     * 加えるか新しい段落を開く
     */
    if (!close_to(parser, matched))
    {
        return false;
    }
    record_blank_line(parser, &line);
    if (accepts_lines(open_tip(parser)))
    {
        return add_line_to_code_or_html(parser, &line);
    }
    if (open_tip(parser)->kind == MND_MD_PARAGRAPH)
    {
        return leaf_add_line(parser, &line, line.indent.pos, line.phys.line_end, 0, false);
    }
    if (line.indent.blank)
    {
        return true;
    }
    if (!open_paragraph(parser, &line))
    {
        return false;
    }

    return leaf_add_line(parser, &line, line.indent.pos, line.phys.line_end, 0, false);
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
