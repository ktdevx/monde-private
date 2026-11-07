/* FrontMatterSegments: キーパスの照合、スカラーの判定、先頭・末尾の改行の除去、原文順の整列 */

#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "yaml/yaml.h"
#include "yaml/yaml_path.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* ラベルの接頭辞（ラベルの規則） */
static const char tu_fm_label_prefix[] = "front matter: ";

/* text[0..len) が空、または空白・タブ・改行だけなら true */
static bool fm_is_blank(const char *text, size_t len)
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

/* doc のセグメントの first 番目以降に scalar を指すものがあれば true */
static bool fm_has_scalar(const mnd_tu_doc_t *doc, size_t first, const mnd_yaml_node_t *scalar)
{
    size_t i;

    for (i = first; i < doc->seg_count; i++)
    {
        if (doc->segs[i].scalar == scalar)
        {
            return true;
        }
    }
    return false;
}

/*
 * スカラー node から 1 つのセグメントを作って doc に加える。論理値が空・空白だけなら加えない。
 * メモリ不足なら false（診断は加えない。追加途中のセグメントは mnd_tu_doc_free が解放する）
 */
static bool fm_add_scalar(mnd_tu_doc_t *doc, const mnd_yaml_node_t *node, const char *key, size_t key_len)
{
    const char *value = node->as.scalar.value;
    size_t start = 0;
    size_t end = node->as.scalar.value_len;
    mnd_tu_seg_t *seg;

    /* 先頭と末尾の \n の並びを除く（2.7） */
    while ((start < end) && (value[start] == '\n'))
    {
        start++;
    }
    while ((end > start) && (value[end - 1] == '\n'))
    {
        end--;
    }
    if (fm_is_blank(value + start, end - start))
    {
        return true;
    }

    seg = mnd_tu_doc_push_segment(doc);
    if (seg == NULL)
    {
        return false;
    }
    seg->pub.origin = MND_TU_ORIGIN_FRONT_MATTER;
    seg->pub.span = node->span;
    seg->pub.line = node->start_line;
    seg->pub.lead_newlines = start;
    seg->pub.trail_newlines = node->as.scalar.value_len - end;
    seg->scalar = node;
    if (!mnd_buf_append(&seg->pub.msgid, value + start, end - start))
    {
        return false;
    }
    if (!mnd_buf_append(&seg->pub.kind, tu_fm_label_prefix, sizeof(tu_fm_label_prefix) - 1))
    {
        return false;
    }
    if (!mnd_buf_append(&seg->pub.kind, key, key_len))
    {
        return false;
    }
    return true;
}

/* doc のセグメントの first 番目以降を span.start の昇順に並べる（挿入ソート。同じ位置は元の順を保つ） */
static void fm_sort_segments(mnd_tu_doc_t *doc, size_t first)
{
    size_t i;

    for (i = first + 1; i < doc->seg_count; i++)
    {
        mnd_tu_seg_t moving = doc->segs[i];
        size_t j = i;

        while ((j > first) && (doc->segs[j - 1].pub.span.start > moving.pub.span.start))
        {
            doc->segs[j] = doc->segs[j - 1];
            j--;
        }
        doc->segs[j] = moving;
    }
}

/*
 * mnd_yaml_path_find を呼ぶ。上流の照合は path を持たないので、診断は一時的な一覧で受け取り、
 * 重大度・行番号・メッセージはそのままに、path が NULL のものには走査の path を添えて diags に移す（7.6）
 */
static bool fm_path_find(const mnd_tu_doc_t *doc, const char *key, size_t key_len,
                         mnd_yaml_matches_t *matches, mnd_diag_list_t *diags)
{
    mnd_diag_list_t local = MND_DIAG_LIST_INIT;
    bool ok = mnd_yaml_path_find(doc->yaml, key, key_len, matches, &local);
    size_t i;

    for (i = 0; i < mnd_diag_list_count(&local); i++)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(&local, i);
        const char *path = (diag->path != NULL) ? diag->path : doc->path;

        mnd_diag_add(diags, diag->severity, path, diag->line, "%s", diag->message);
    }
    /* 一時的な一覧でメモリ不足により記録できなかった診断があれば、そのことも引き継ぐ */
    if (local.dropped)
    {
        diags->dropped = true;
    }
    mnd_diag_list_free(&local);
    return ok;
}

bool mnd_tu_collect_front_matter(mnd_tu_doc_t *doc, const char *const *keys, size_t key_count,
                                 mnd_diag_list_t *diags)
{
    size_t first = doc->seg_count;
    size_t k;

    for (k = 0; k < key_count; k++)
    {
        /* NULL のキーパスは空のキーパスとして扱い、上流が不正なキーパスとして報告する */
        const char *key = ((keys != NULL) && (keys[k] != NULL)) ? keys[k] : "";
        size_t key_len = strlen(key);
        mnd_yaml_matches_t matches = { NULL, 0 };
        size_t i;

        /* キーパスはそのまま渡す。不正なキーパスとメモリ不足は上流がエラーを追加する（7.3） */
        if (!fm_path_find(doc, key, key_len, &matches, diags))
        {
            return false;
        }
        for (i = 0; i < matches.count; i++)
        {
            const mnd_yaml_node_t *node = matches.nodes[i];

            if (node->kind != MND_YAML_SCALAR)
            {
                /* スカラー以外は警告して飛ばす（1.11） */
                mnd_diag_add(diags, MND_SEVERITY_WARNING, doc->path, node->start_line,
                             "front matter key \"%s\" is not a scalar", key);
                continue;
            }
            /* 同じスカラーは最初のキーパスのセグメントだけにする */
            if (fm_has_scalar(doc, first, node))
            {
                continue;
            }
            if (!fm_add_scalar(doc, node, key, key_len))
            {
                mnd_yaml_matches_free(doc->yaml, &matches);
                return false;
            }
        }
        mnd_yaml_matches_free(doc->yaml, &matches);
    }
    /* 照合はキーパスごとに原文の順なので、キーパスをまたいで原文の順に並べる */
    fm_sort_segments(doc, first);
    return true;
}
