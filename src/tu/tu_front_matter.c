/* FrontMatterSegments: キーパスの照合、スカラーの判定、先頭・末尾の改行の除去、原文順の整列 */

#include "tu/tu_internal.h"

#include "foundation/diag.h"

/* 現在はキーパスを照合せず、セグメントを加えずに成功を返す */
bool mnd_tu_collect_front_matter(mnd_tu_doc_t *doc, const char *const *keys, size_t key_count,
                                 mnd_diag_list_t *diags)
{
    (void)doc;
    (void)keys;
    (void)key_count;
    (void)diags;
    return true;
}
