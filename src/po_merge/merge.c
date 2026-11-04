/* Merge: 入力の検査、各段階の呼び出し、出力の順序、統計、失敗時の後始末 */

#include "po_merge/merge.h"
#include "po_merge/merge_internal.h"

/*
 * 仮の実装: マージの手順を実装するまで、出力を空に、統計を 0 にして false を返す
 */
bool mnd_merge_catalogs(const mnd_po_catalog_t *template_catalog, const char *template_path,
                        const mnd_po_catalog_t *old_catalog, const char *old_path,
                        const mnd_merge_options_t *options,
                        mnd_po_catalog_t *out, mnd_merge_stats_t *stats,
                        mnd_diag_list_t *diags)
{
    (void)template_catalog;
    (void)template_path;
    (void)old_catalog;
    (void)old_path;
    (void)options;
    (void)diags;

    mnd_po_catalog_free(out);
    mnd_po_catalog_init(out);
    if (stats != NULL)
    {
        stats->matched = 0;
        stats->fuzzy = 0;
        stats->added = 0;
        stats->obsoleted = 0;
    }

    return false;
}
