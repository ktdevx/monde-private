/* Scanner: 検出 → 解析 → セグメントの列の組み立て、mnd_tu_doc_t の生成と解放 */

#include "tu/tu.h"
#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "markdown/md.h"
#include "markdown/md_parse.h"
#include "yaml/yaml.h"
#include "yaml/yaml_front_matter.h"
#include "yaml/yaml_parse.h"

#include <stdint.h>
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

/* セグメントの配列の最初の容量 */
#define TU_SCAN_MIN_SEG_CAP 16

mnd_tu_seg_t *mnd_tu_doc_push_segment(mnd_tu_doc_t *doc)
{
    mnd_tu_seg_t *seg;

    if (doc->seg_count == doc->seg_cap)
    {
        size_t new_cap;
        mnd_tu_seg_t *grown;

        if (doc->seg_cap == 0)
        {
            new_cap = TU_SCAN_MIN_SEG_CAP;
        }
        else
        {
            if (doc->seg_cap > (SIZE_MAX / 2))
            {
                return NULL;
            }
            new_cap = doc->seg_cap * 2;
        }
        if (new_cap > (SIZE_MAX / sizeof(*grown)))
        {
            return NULL;
        }
        grown = (mnd_tu_seg_t *)realloc(doc->segs, new_cap * sizeof(*grown));
        if (grown == NULL)
        {
            return NULL;
        }
        doc->segs = grown;
        doc->seg_cap = new_cap;
    }
    seg = &doc->segs[doc->seg_count];
    seg->pub.origin = MND_TU_ORIGIN_MARKDOWN;
    seg->pub.span.start = 0;
    seg->pub.span.end = 0;
    seg->pub.line = 0;
    mnd_buf_init(&seg->pub.msgid);
    mnd_buf_init(&seg->pub.kind);
    seg->pub.lead_newlines = 0;
    seg->pub.trail_newlines = 0;
    seg->node = NULL;
    seg->scalar = NULL;
    doc->seg_count++;
    return seg;
}

/* diags の first 番目以降にエラーの診断があれば true */
static bool scan_has_error_since(const mnd_diag_list_t *diags, size_t first)
{
    size_t i;

    for (i = first; i < mnd_diag_list_count(diags); i++)
    {
        if (mnd_diag_list_at(diags, i)->severity == MND_SEVERITY_ERROR)
        {
            return true;
        }
    }
    return false;
}

/*
 * 走査の規則の順に上流の解析とセグメントの収集を行う。
 * 失敗したら false（上流の診断はそのまま残し、診断のない失敗はメモリ不足として扱う）。
 * 確保したものは doc が持ち、呼び出し側が mnd_tu_doc_free で解放する
 */
static bool scan_run(mnd_tu_doc_t *doc, const mnd_tu_options_t *options, mnd_diag_list_t *diags)
{
    const char *path = (options != NULL) ? options->path : NULL;
    const char *const *keys = (options != NULL) ? options->front_matter_keys : NULL;
    size_t key_count = (options != NULL) ? options->front_matter_key_count : 0;
    mnd_yaml_front_matter_t fm;
    mnd_md_options_t md_options = MND_MD_OPTIONS_INIT;
    size_t first = mnd_diag_list_count(diags);

    /* 不正な UTF-8 は文書全体を検査する検出が報告する（7.1） */
    if (!mnd_yaml_front_matter_detect(doc->data, doc->len, path, &fm, diags))
    {
        return false;
    }

    /* キーパスの指定がなければ YAML は解析せず、body_start だけを使う（1.9） */
    if ((fm.present) && (key_count > 0))
    {
        mnd_yaml_options_t yaml_options = MND_YAML_OPTIONS_INIT;

        yaml_options.path = path;
        if (!mnd_yaml_parse(doc->data, doc->len, fm.content, &yaml_options, &doc->yaml, diags))
        {
            doc->yaml = NULL;
            return false;
        }
        if (!mnd_tu_collect_front_matter(doc, keys, key_count, diags))
        {
            if (!scan_has_error_since(diags, first))
            {
                mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "out of memory");
            }
            return false;
        }
    }

    md_options.body_start = fm.body_start;
    md_options.path = path;
    if (!mnd_md_parse(doc->data, doc->len, &md_options, &doc->md, diags))
    {
        doc->md = NULL;
        return false;
    }

    /* MarkdownSegments の失敗はメモリ不足だけで、診断は本関数が追加する */
    if (!mnd_tu_collect_markdown(doc))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, path, 0, "out of memory");
        return false;
    }
    return true;
}

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
    if (!scan_run(doc, options, diags))
    {
        /* それまでに確保したもの（yaml / md の文書、セグメント、パス）をすべて解放する */
        mnd_tu_doc_free(doc);
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
