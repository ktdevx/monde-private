/* Rebuilder: カタログの検索、状態と統計、訳文の検査、短絡、出力の組み立て */

#include "tu/tu.h"

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "po/po.h"

/* 現在は訳文を差し戻さず、out を空にし *stats を 0 にして失敗を返す */
bool mnd_tu_translate(const mnd_tu_doc_t *doc, const mnd_po_catalog_t *catalog,
                      const mnd_tu_translate_options_t *options,
                      mnd_buf_t *out, mnd_tu_stats_t *stats, mnd_diag_list_t *diags)
{
    static const mnd_tu_stats_t zero = MND_TU_STATS_INIT;

    (void)doc;
    (void)catalog;
    (void)options;
    (void)diags;
    mnd_buf_clear(out);
    if (stats != NULL)
    {
        *stats = zero;
    }
    return false;
}
