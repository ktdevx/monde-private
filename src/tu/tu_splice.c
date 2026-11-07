/* Splicer: Markdown の行ごとの書き戻し（プレフィックスと改行の復元）、フロントマターの整形の呼び出し */

#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"
#include "markdown/md.h"
#include "yaml/yaml.h"
#include "yaml/yaml_format.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* out の長さを len に戻す（失敗時に追加途中の内容を取り除く） */
static void splice_truncate(mnd_buf_t *out, size_t len)
{
    if (out->data != NULL)
    {
        out->len = len;
        out->data[len] = '\0';
    }
}

/*
 * 原文より多い行に付ける組み立てたプレフィックスを out の後ろに追加する（Markdown の書き戻し）。
 * 2 行以上あり最後の行が遅延継続行でなければ、その行のプレフィックスの原文のバイト列。
 * そうでなければ、根から葉への祖先の引用に "> "、リスト項目に content_indent 個の空白を連結したもの。
 * メモリ不足なら false
 */
static bool splice_append_built_prefix(const mnd_tu_doc_t *doc, const mnd_md_node_t *node, mnd_buf_t *out)
{
    const mnd_md_line_t *last = &node->lines[node->line_count - 1];
    const mnd_md_node_t *ancestor;
    const mnd_md_node_t *chain_end = NULL;

    if ((node->line_count >= 2) && !last->lazy)
    {
        return mnd_buf_append(out, doc->data + last->prefix.start, last->prefix.end - last->prefix.start);
    }

    /*
     * 祖先を根から葉の順に見るため、毎回 葉の親から chain_end の直前まで上る。
     * 祖先の数は入れ子の深さなので、二重の繰り返しでも十分に小さい
     */
    while (chain_end != node->parent)
    {
        ancestor = node->parent;
        while (ancestor->parent != chain_end)
        {
            ancestor = ancestor->parent;
        }
        if (ancestor->kind == MND_MD_BLOCK_QUOTE)
        {
            if (!mnd_buf_append(out, "> ", 2))
            {
                return false;
            }
        }
        else if (ancestor->kind == MND_MD_ITEM)
        {
            size_t i;

            for (i = 0; i < ancestor->as.item.content_indent; i++)
            {
                if (!mnd_buf_append_byte(out, ' '))
                {
                    return false;
                }
            }
        }
        chain_end = ancestor;
    }
    return true;
}

/* 行の境界に使う改行コード（MND_EOL_NONE なら LF）を out の後ろに追加する。メモリ不足なら false */
static bool splice_append_eol(mnd_eol_t eol, mnd_buf_t *out)
{
    if (eol == MND_EOL_CRLF)
    {
        return mnd_buf_append(out, "\r\n", 2);
    }
    return mnd_buf_append_byte(out, '\n');
}

/* Markdown の書き戻しの本体。out には追加途中の内容が残りうる */
static bool splice_markdown_lines(const mnd_tu_doc_t *doc, const mnd_md_node_t *node,
                                  const char *text, size_t len, mnd_eol_t eol, mnd_buf_t *out)
{
    size_t pos = 0;
    size_t k = 0;

    for (;;)
    {
        const char *nl = (pos < len) ? (const char *)memchr(text + pos, '\n', len - pos) : NULL;
        size_t line_end = (nl != NULL) ? (size_t)(nl - text) : len;

        if (k > 0)
        {
            if (k < node->line_count)
            {
                /* 前の行の行末記号 + k 行目のプレフィックス（原文のバイト列） */
                size_t start = node->lines[k - 1].content.end;
                size_t end = node->lines[k].content.start;

                if (!mnd_buf_append(out, doc->data + start, end - start))
                {
                    return false;
                }
            }
            else
            {
                if (!splice_append_eol(eol, out) || !splice_append_built_prefix(doc, node, out))
                {
                    return false;
                }
            }
        }
        if ((line_end > pos) && !mnd_buf_append(out, text + pos, line_end - pos))
        {
            return false;
        }
        if (nl == NULL)
        {
            return true;
        }
        pos = line_end + 1;
        k++;
    }
}

bool mnd_tu_splice_markdown(const mnd_tu_doc_t *doc, const mnd_tu_seg_t *seg,
                            const char *text, size_t len, mnd_eol_t eol, mnd_buf_t *out)
{
    size_t initial = out->len;

    if (!splice_markdown_lines(doc, seg->node, text, len, eol, out))
    {
        splice_truncate(out, initial);
        return false;
    }
    return true;
}

bool mnd_tu_splice_front_matter(const mnd_tu_doc_t *doc, const mnd_tu_seg_t *seg,
                                const char *text, size_t len, mnd_diag_list_t *diags, mnd_buf_t *out)
{
    size_t initial = out->len;
    mnd_buf_t padded = MND_BUF_INIT;
    const char *value = text;
    size_t value_len = len;
    size_t i;
    bool ok;

    /* 走査で除いた先頭と末尾の \n を同じ数だけ補う（2.7） */
    if ((seg->pub.lead_newlines > 0) || (seg->pub.trail_newlines > 0))
    {
        ok = mnd_buf_reserve(&padded, seg->pub.lead_newlines + len + seg->pub.trail_newlines);
        for (i = 0; ok && (i < seg->pub.lead_newlines); i++)
        {
            ok = mnd_buf_append_byte(&padded, '\n');
        }
        ok = ok && mnd_buf_append(&padded, text, len);
        for (i = 0; ok && (i < seg->pub.trail_newlines); i++)
        {
            ok = mnd_buf_append_byte(&padded, '\n');
        }
        if (!ok)
        {
            mnd_buf_free(&padded);
            return false;
        }
        value = padded.data;
        value_len = padded.len;
    }
    else if (value == NULL)
    {
        value = "";
    }

    ok = mnd_yaml_format_value(doc->yaml, seg->scalar, value, value_len, doc->path, out, diags);
    mnd_buf_free(&padded);
    if (!ok)
    {
        splice_truncate(out, initial);
    }
    return ok;
}
