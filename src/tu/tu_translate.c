/* Rebuilder: カタログの検索、状態と統計、訳文の検査、短絡、出力の組み立て */

#include "tu/tu.h"
#include "tu/tu_internal.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/eol.h"
#include "foundation/utf8.h"
#include "markdown/md.h"
#include "po/po.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * セグメントの検索と状態の判定（検索と状態）。統計を数え、採用する訳文を返す。
 * 原文を出すとき（該当なし・未翻訳・不採用の fuzzy）は NULL。*entry_out に一致したエントリ（なければ NULL）を入れる
 */
static const mnd_buf_t *translate_lookup(const mnd_tu_seg_t *seg, const mnd_po_catalog_t *catalog,
                                         bool use_fuzzy, mnd_tu_stats_t *counts,
                                         const mnd_po_entry_t **entry_out)
{
    const mnd_po_entry_t *entry;
    size_t index;

    *entry_out = NULL;
    counts->total++;
    if (!mnd_po_catalog_find(catalog, NULL, mnd_buf_cstr(&seg->pub.msgid), &index))
    {
        counts->missing++;
        return NULL;
    }
    entry = &catalog->entries[index];
    *entry_out = entry;
    switch (mnd_po_entry_state(entry))
    {
    case MND_PO_STATE_TRANSLATED:
        counts->translated++;
        return &entry->msgstr.items[0];
    case MND_PO_STATE_FUZZY:
        counts->fuzzy++;
        return use_fuzzy ? &entry->msgstr.items[0] : NULL;
    case MND_PO_STATE_UNTRANSLATED:
    default:
        counts->untranslated++;
        return NULL;
    }
}

/* 採用した訳文の UTF-8 と NUL を検査する（訳文の検査）。不正ならエラーを追加して false */
static bool translate_validate(const mnd_buf_t *msgstr, const mnd_po_entry_t *entry,
                               const char *catalog_path, mnd_diag_list_t *diags)
{
    size_t invalid_offset;

    if (!mnd_utf8_validate(msgstr->data, msgstr->len, &invalid_offset))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, catalog_path, entry->line,
                     "invalid UTF-8 sequence in translation");
        return false;
    }
    if ((msgstr->len > 0) && (strlen(msgstr->data) != msgstr->len))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, catalog_path, entry->line, "NUL in translation");
        return false;
    }
    return true;
}

/* Markdown のセグメントの訳文を StructureCheck で検査し、当てはまれば警告を追加する（構造の検査） */
static void translate_check(const mnd_tu_doc_t *doc, const mnd_tu_seg_t *seg, const mnd_buf_t *msgstr,
                            mnd_diag_list_t *diags)
{
    bool is_atx_heading = (seg->node->kind == MND_MD_HEADING) && !seg->node->as.heading.setext;
    size_t line_index = 0;

    switch (mnd_tu_check_structure(mnd_buf_cstr(msgstr), msgstr->len, is_atx_heading, &line_index))
    {
    case MND_TU_CHECK_BLANK_LINE:
        mnd_diag_add(diags, MND_SEVERITY_WARNING, doc->path, seg->pub.line,
                     "translation contains a blank line (msgid at line %zu)", seg->pub.line);
        break;
    case MND_TU_CHECK_HEADING_LINE_BREAK:
        mnd_diag_add(diags, MND_SEVERITY_WARNING, doc->path, seg->pub.line,
                     "translation of a one-line heading contains a line break (msgid at line %zu)",
                     seg->pub.line);
        break;
    case MND_TU_CHECK_BLOCK_MARKER:
        /* 訳文の行番号は人が読むため 1 起点にする */
        mnd_diag_add(diags, MND_SEVERITY_WARNING, doc->path, seg->pub.line,
                     "translation line %zu starts with a block marker (msgid at line %zu)",
                     line_index + 1, seg->pub.line);
        break;
    case MND_TU_CHECK_OK:
    default:
        break;
    }
}

/*
 * セグメント seg の置換のバイト列を out の後ろに追加する（Splicer）。
 * 失敗なら false。診断を加えずに失敗したとき（メモリ不足）は out of memory を追加する
 */
static bool translate_splice(const mnd_tu_doc_t *doc, const mnd_tu_seg_t *seg, const mnd_buf_t *msgstr,
                             mnd_eol_t eol, mnd_buf_t *out, mnd_diag_list_t *diags)
{
    size_t diag_count = mnd_diag_list_count(diags);
    bool ok;

    if (seg->pub.origin == MND_TU_ORIGIN_FRONT_MATTER)
    {
        ok = mnd_tu_splice_front_matter(doc, seg, mnd_buf_cstr(msgstr), msgstr->len, diags, out);
    }
    else
    {
        ok = mnd_tu_splice_markdown(doc, seg, mnd_buf_cstr(msgstr), msgstr->len, eol, out);
    }
    if (!ok && (mnd_diag_list_count(diags) == diag_count))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, doc->path, 0, "out of memory");
    }
    return ok;
}

/* 再構成の本体。失敗時の out と統計の後始末は呼び出し側が行う */
static bool translate_rebuild(const mnd_tu_doc_t *doc, const mnd_po_catalog_t *catalog,
                              const mnd_tu_translate_options_t *options,
                              mnd_buf_t *out, mnd_tu_stats_t *counts, mnd_diag_list_t *diags)
{
    size_t pos = 0;
    size_t i;

    for (i = 0; i < doc->seg_count; i++)
    {
        const mnd_tu_seg_t *seg = &doc->segs[i];
        const mnd_po_entry_t *entry;
        const mnd_buf_t *msgstr = translate_lookup(seg, catalog, options->use_fuzzy, counts, &entry);
        bool copy_source = true;

        if (msgstr != NULL)
        {
            if (!translate_validate(msgstr, entry, options->catalog_path, diags))
            {
                return false;
            }
            /* 短絡: msgid と同じ訳文は置換も検査もせず、原文の範囲を出す */
            copy_source = (msgstr->len == seg->pub.msgid.len)
                          && (memcmp(msgstr->data, seg->pub.msgid.data, msgstr->len) == 0);
        }

        if (!mnd_buf_append(out, doc->data + pos, seg->pub.span.start - pos))
        {
            mnd_diag_add(diags, MND_SEVERITY_ERROR, doc->path, 0, "out of memory");
            return false;
        }
        if (copy_source)
        {
            if (!mnd_buf_append(out, doc->data + seg->pub.span.start,
                                seg->pub.span.end - seg->pub.span.start))
            {
                mnd_diag_add(diags, MND_SEVERITY_ERROR, doc->path, 0, "out of memory");
                return false;
            }
        }
        else
        {
            if (seg->pub.origin == MND_TU_ORIGIN_MARKDOWN)
            {
                translate_check(doc, seg, msgstr, diags);
            }
            if (!translate_splice(doc, seg, msgstr, options->eol, out, diags))
            {
                return false;
            }
        }
        pos = seg->pub.span.end;
    }

    /* len == 0 では data が NULL でありうるので、残りがあるときだけコピーする */
    if ((doc->len > pos) && !mnd_buf_append(out, doc->data + pos, doc->len - pos))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, doc->path, 0, "out of memory");
        return false;
    }
    return true;
}

bool mnd_tu_translate(const mnd_tu_doc_t *doc, const mnd_po_catalog_t *catalog,
                      const mnd_tu_translate_options_t *options,
                      mnd_buf_t *out, mnd_tu_stats_t *stats, mnd_diag_list_t *diags)
{
    static const mnd_tu_translate_options_t default_options = MND_TU_TRANSLATE_OPTIONS_INIT;
    static const mnd_tu_stats_t zero = MND_TU_STATS_INIT;
    mnd_tu_stats_t counts = MND_TU_STATS_INIT;
    bool ok;

    mnd_buf_clear(out);
    if (options == NULL)
    {
        options = &default_options;
    }
    ok = translate_rebuild(doc, catalog, options, out, &counts, diags);
    if (!ok)
    {
        mnd_buf_clear(out);
        counts = zero;
    }
    if (stats != NULL)
    {
        *stats = counts;
    }
    return ok;
}
