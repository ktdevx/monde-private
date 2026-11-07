/* Splicer: Markdown の行ごとの書き戻し（プレフィックスと改行の復元）、フロントマターの整形の呼び出し */

#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"

/* 現在は置換のバイト列を作らず、out を変えずに失敗を返す */
bool mnd_tu_splice_markdown(const mnd_tu_doc_t *doc, const mnd_tu_seg_t *seg,
                            const char *text, size_t len, mnd_eol_t eol, mnd_buf_t *out)
{
    (void)doc;
    (void)seg;
    (void)text;
    (void)len;
    (void)eol;
    (void)out;
    return false;
}

/* 現在は置換のバイト列を作らず、out を変えずに失敗を返す */
bool mnd_tu_splice_front_matter(const mnd_tu_doc_t *doc, const mnd_tu_seg_t *seg,
                                const char *text, size_t len, mnd_diag_list_t *diags, mnd_buf_t *out)
{
    (void)doc;
    (void)seg;
    (void)text;
    (void)len;
    (void)diags;
    (void)out;
    return false;
}
