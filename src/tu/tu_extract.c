/* Extractor: msgid の統合、エントリの順序、#: と #. の付与 */

#include "tu/tu.h"
#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"

#include <stdlib.h>
#include <string.h>

/* 呼び出し前から out にあったエントリの、参照と抽出コメントの件数（失敗時に戻すため） */
typedef struct extract_saved
{
    size_t ref_count;
    size_t comment_count;
} extract_saved_t;

/* list に data[0..len) と同じ文字列があるか */
static bool extract_strings_contain(const mnd_po_strings_t *list, const char *data, size_t len)
{
    size_t i;

    for (i = 0; i < list->count; i++)
    {
        if ((list->items[i].len == len) && ((len == 0) || (memcmp(list->items[i].data, data, len) == 0)))
        {
            return true;
        }
    }
    return false;
}

/*
 * セグメント seg を out に加える（抽出の規則の 1〜4）。
 * msgctxt のない同じ msgid のエントリがあれば参照とラベルを足し、なければ新しいエントリを末尾に加える。
 * メモリ不足なら false
 */
static bool extract_segment(const mnd_tu_segment_t *seg, const char *ref_file, mnd_po_catalog_t *out)
{
    size_t index;
    mnd_po_entry_t *entry;

    if (mnd_po_catalog_find(out, NULL, mnd_buf_cstr(&seg->msgid), &index))
    {
        entry = &out->entries[index];
    }
    else
    {
        /* 新しいエントリ: msgstr は要素なし（ライターが msgstr "" を書く）、フラグなし */
        entry = mnd_po_catalog_append(out);
        if (entry == NULL)
        {
            return false;
        }
        if (!mnd_buf_append(&entry->msgid, seg->msgid.data, seg->msgid.len))
        {
            return false;
        }
    }

    if (!mnd_po_refs_push(&entry->refs, ref_file, seg->line))
    {
        return false;
    }
    if (extract_strings_contain(&entry->extracted_comments, seg->kind.data, seg->kind.len))
    {
        return true;
    }
    return mnd_po_strings_push(&entry->extracted_comments, mnd_buf_cstr(&seg->kind), seg->kind.len);
}

/* list の末尾の要素を解放して count 件に戻す */
static void extract_truncate_strings(mnd_po_strings_t *list, size_t count)
{
    while (list->count > count)
    {
        list->count--;
        mnd_buf_free(&list->items[list->count]);
    }
}

static void extract_truncate_refs(mnd_po_refs_t *refs, size_t count)
{
    while (refs->count > count)
    {
        refs->count--;
        mnd_buf_free(&refs->items[refs->count].file);
    }
}

/*
 * out を呼び出し前の状態に戻す。first_new 以降のエントリを解放して件数を戻し、
 * それより前のエントリは saved（first_new == 0 なら NULL 可）の件数まで参照と抽出コメントを取り除く
 */
static void extract_rollback(mnd_po_catalog_t *out, size_t first_new, const extract_saved_t *saved)
{
    size_t i;

    while (out->count > first_new)
    {
        out->count--;
        mnd_po_entry_free(&out->entries[out->count]);
    }
    for (i = 0; i < first_new; i++)
    {
        extract_truncate_refs(&out->entries[i].refs, saved[i].ref_count);
        extract_truncate_strings(&out->entries[i].extracted_comments, saved[i].comment_count);
    }
}

bool mnd_tu_extract(const mnd_tu_doc_t *doc, const char *ref_file,
                    mnd_po_catalog_t *out, mnd_diag_list_t *diags)
{
    size_t first_new = out->count;
    size_t seg_count = mnd_tu_doc_segment_count(doc);
    extract_saved_t *saved = NULL;
    size_t i;

    /* 既存のエントリにも参照と抽出コメントを足すため、失敗時に戻す件数を控える */
    if ((first_new > 0) && (seg_count > 0))
    {
        saved = (extract_saved_t *)malloc(first_new * sizeof(*saved));
        if (saved == NULL)
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, doc->path, 0, "out of memory");
            return false;
        }
        for (i = 0; i < first_new; i++)
        {
            saved[i].ref_count = out->entries[i].refs.count;
            saved[i].comment_count = out->entries[i].extracted_comments.count;
        }
    }

    for (i = 0; i < seg_count; i++)
    {
        if (!extract_segment(mnd_tu_doc_segment(doc, i), ref_file, out))
        {
            extract_rollback(out, first_new, saved);
            free(saved);
            mnd_diag_add(diags, MND_SEVERITY_ERROR, doc->path, 0, "out of memory");
            return false;
        }
    }

    free(saved);
    return true;
}
