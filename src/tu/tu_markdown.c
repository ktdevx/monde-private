/* MarkdownSegments: 構文木の走査、翻訳対象の判定、範囲と msgid と #. のラベル */

#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "markdown/md.h"

/* 現在は構文木を走査せず、セグメントを加えずに成功を返す */
bool mnd_tu_collect_markdown(mnd_tu_doc_t *doc)
{
    (void)doc;
    return true;
}

/* 現在はラベルを追加せず、成功を返す */
bool mnd_tu_kind_label(const mnd_md_node_t *node, mnd_buf_t *out)
{
    (void)node;
    (void)out;
    return true;
}
