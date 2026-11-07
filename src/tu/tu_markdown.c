/* MarkdownSegments: 構文木の走査、翻訳対象の判定、範囲と msgid と #. のラベル */

#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "markdown/md.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* 前順で node の部分木を飛ばした次のノード。root の部分木を出たら NULL */
static const mnd_md_node_t *markdown_skip_subtree(const mnd_md_node_t *node, const mnd_md_node_t *root)
{
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

/* text[0..len) が空、または空白・タブ・改行だけなら true（1.6） */
static bool markdown_is_blank(const char *text, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++)
    {
        if ((text[i] != ' ') && (text[i] != '\t') && (text[i] != '\n'))
        {
            return false;
        }
    }
    return true;
}

/*
 * 段落または見出しの node をセグメントにして doc に加える。論理テキストが空か空白だけなら加えない。
 * メモリ不足なら false
 */
static bool markdown_add_segment(mnd_tu_doc_t *doc, const mnd_md_node_t *node)
{
    const mnd_md_line_t *last;
    mnd_span_t span;
    mnd_buf_t msgid;
    mnd_tu_seg_t *seg;

    /* パーサの不変条件では line_count は 1 以上だが、行のないノードは飛ばす */
    if ((node->lines == NULL) || (node->line_count == 0))
    {
        return true;
    }

    /* 範囲は最初の行の内容の開始から、最後の行の内容の終わりから空白とタブを除いた位置まで（2.3） */
    last = &node->lines[node->line_count - 1];
    span.start = node->lines[0].content.start;
    span.end = last->content.end;
    while ((span.end > last->content.start)
           && ((doc->data[span.end - 1] == ' ') || (doc->data[span.end - 1] == '\t')))
    {
        span.end--;
    }

    /* msgid は論理テキスト（行頭のプレフィックスを除き、行の境界は \n）（2.1、2.2、2.4、2.5） */
    mnd_buf_init(&msgid);
    if (!mnd_md_logical_text(doc->md, node, span, &msgid))
    {
        mnd_buf_free(&msgid);
        return false;
    }
    if (markdown_is_blank(msgid.data, msgid.len))
    {
        mnd_buf_free(&msgid);
        return true;
    }

    seg = mnd_tu_doc_push_segment(doc);
    if (seg == NULL)
    {
        mnd_buf_free(&msgid);
        return false;
    }
    /* 一時的なバッファの所有を移す（push_segment が初期化した空のバッファは何も持たない） */
    seg->pub.origin = MND_TU_ORIGIN_MARKDOWN;
    seg->pub.span = span;
    seg->pub.line = node->start_line;
    seg->pub.msgid = msgid;
    seg->node = node;
    return mnd_tu_kind_label(node, &seg->pub.kind);
}

bool mnd_tu_collect_markdown(mnd_tu_doc_t *doc)
{
    const mnd_md_node_t *root;
    const mnd_md_node_t *node;

    root = mnd_md_doc_root(doc->md);
    node = root;
    while (node != NULL)
    {
        switch (node->kind)
        {
        case MND_MD_PARAGRAPH:
        case MND_MD_HEADING:
            /* セグメントの候補。子（インライン）には入らない（1.1、1.2） */
            if (!markdown_add_segment(doc, node))
            {
                return false;
            }
            node = markdown_skip_subtree(node, root);
            break;
        case MND_MD_CODE_BLOCK:
        case MND_MD_HTML_BLOCK:
        case MND_MD_LINK_REF_DEF:
        case MND_MD_THEMATIC_BREAK:
            /* 翻訳対象にしない（1.5） */
            node = markdown_skip_subtree(node, root);
            break;
        default:
            /* コンテナと方言の種類のノードは、自身はセグメントにせず子を走査する（1.7） */
            node = mnd_md_node_next(node, root);
            break;
        }
    }
    return true;
}

bool mnd_tu_kind_label(const mnd_md_node_t *node, mnd_buf_t *out)
{
    const mnd_md_node_t *p;
    const mnd_md_node_t **chain;
    size_t depth = 0;
    size_t i;
    bool first = true;

    /* 根から葉の順に並べるため、ラベルを出す祖先を葉から根へ集める */
    for (p = node->parent; p != NULL; p = p->parent)
    {
        if ((p->kind == MND_MD_BLOCK_QUOTE) || (p->kind == MND_MD_ITEM))
        {
            depth++;
        }
    }
    chain = NULL;
    if (depth > 0)
    {
        chain = (const mnd_md_node_t **)malloc(depth * sizeof(*chain));
        if (chain == NULL)
        {
            return false;
        }
        i = depth;
        for (p = node->parent; p != NULL; p = p->parent)
        {
            if ((p->kind == MND_MD_BLOCK_QUOTE) || (p->kind == MND_MD_ITEM))
            {
                i--;
                chain[i] = p;
            }
        }
    }

    for (i = 0; i < depth; i++)
    {
        if (!first)
        {
            if (!mnd_buf_append_str(out, " > "))
            {
                free((void *)chain);
                return false;
            }
        }
        first = false;
        if (!mnd_buf_append_str(out, (chain[i]->kind == MND_MD_BLOCK_QUOTE) ? "block quote" : "list item"))
        {
            free((void *)chain);
            return false;
        }
    }
    free((void *)chain);

    if (!first)
    {
        if (!mnd_buf_append_str(out, " > "))
        {
            return false;
        }
    }
    if (node->kind == MND_MD_HEADING)
    {
        char label[32];

        (void)snprintf(label, sizeof(label), "heading (level %d)", node->as.heading.level);
        return mnd_buf_append_str(out, label);
    }
    return mnd_buf_append_str(out, "paragraph");
}
