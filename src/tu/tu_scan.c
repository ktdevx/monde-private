/* Scanner: 検出 → 解析 → セグメントの列の組み立て、mnd_tu_doc_t の生成と解放 */

#include "tu/tu.h"
#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "markdown/md.h"
#include "yaml/yaml.h"

#include <stdlib.h>
#include <string.h>

/* path の複製を *out に入れる。path が NULL なら *out も NULL。メモリ不足なら false */
static bool scan_copy_path(const char *path, char **out)
{
    size_t len;
    char *copy;

    *out = NULL;
    if (path == NULL)
    {
        return true;
    }
    len = strlen(path);
    copy = (char *)malloc(len + 1);
    if (copy == NULL)
    {
        return false;
    }
    memcpy(copy, path, len + 1);
    *out = copy;
    return true;
}

/*
 * 現在の走査は、原文とパスを持つ空の文書を作るだけで、上流の解析とセグメントの収集は行わない
 * （セグメントは常に 0 件）
 */
bool mnd_tu_scan(const char *data, size_t len, const mnd_tu_options_t *options,
                 mnd_tu_doc_t **out, mnd_diag_list_t *diags)
{
    const char *path = (options != NULL) ? options->path : NULL;
    mnd_tu_doc_t *doc;

    *out = NULL;
    doc = (mnd_tu_doc_t *)calloc(1, sizeof(*doc));
    if (doc == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "out of memory");
        return false;
    }
    doc->data = data;
    doc->len = len;
    if (!scan_copy_path(path, &doc->path))
    {
        free(doc);
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "out of memory");
        return false;
    }
    *out = doc;
    return true;
}

size_t mnd_tu_doc_segment_count(const mnd_tu_doc_t *doc)
{
    return doc->seg_count;
}

const mnd_tu_segment_t *mnd_tu_doc_segment(const mnd_tu_doc_t *doc, size_t index)
{
    return &doc->segs[index].pub;
}

void mnd_tu_doc_free(mnd_tu_doc_t *doc)
{
    size_t i;

    if (doc == NULL)
    {
        return;
    }
    for (i = 0; i < doc->seg_count; i++)
    {
        mnd_buf_free(&doc->segs[i].pub.msgid);
        mnd_buf_free(&doc->segs[i].pub.kind);
    }
    free(doc->segs);
    mnd_yaml_doc_free(doc->yaml);
    mnd_md_doc_free(doc->md);
    free(doc->path);
    free(doc);
}
