/* Merge: 手順の実行、出力の順序、統計、エラー */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "po/po.h"
#include "po_merge/merge.h"

NAOS_TEST(merge_scaffold_failure_leaves_output_empty_and_stats_zero)
{
    mnd_po_catalog_t template_catalog;
    mnd_po_catalog_t old_catalog;
    mnd_po_catalog_t out;
    mnd_po_entry_t *entry;
    mnd_merge_stats_t stats = { 1, 2, 3, 4 };
    mnd_diag_list_t diags;
    bool ok;

    mnd_po_catalog_init(&template_catalog);
    mnd_po_catalog_init(&old_catalog);
    mnd_po_catalog_init(&out);
    mnd_diag_list_init(&diags);
    entry = mnd_po_catalog_append(&template_catalog);
    NAOS_CHECK_TRUE(entry != NULL);

    /* 仮の実装: 出力を空に、統計を 0 にして false を返す */
    ok = mnd_merge_catalogs(&template_catalog, "template.pot", &old_catalog, "old.po", NULL,
                            &out, &stats, &diags);
    NAOS_CHECK_FALSE(ok);
    NAOS_CHECK_UINT_EQ(out.count, 0);
    NAOS_CHECK_UINT_EQ(stats.matched, 0);
    NAOS_CHECK_UINT_EQ(stats.fuzzy, 0);
    NAOS_CHECK_UINT_EQ(stats.added, 0);
    NAOS_CHECK_UINT_EQ(stats.obsoleted, 0);

    mnd_diag_list_free(&diags);
    mnd_po_catalog_free(&out);
    mnd_po_catalog_free(&old_catalog);
    mnd_po_catalog_free(&template_catalog);
}

NAOS_SUITE(merge,
    NAOS_ADD_TEST(merge_scaffold_failure_leaves_output_empty_and_stats_zero)
)
