/* Merge: 手順の実行、出力の順序、統計、エラー */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "po/po.h"
#include "po_merge/merge.h"
#include "test_util.h"

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

NAOS_TEST(merge_scaffold_test_util_round_trips_po)
{
    static const char po[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"Plural-Forms: nplurals=1; plural=0;\\n\"\n"
        "\n"
        "#. TRANSLATORS: greeting\n"
        "#: src/a.c:10\n"
        "#, c-format\n"
        "msgid \"Hello, %s\"\n"
        "msgstr \"こんにちは、%s\"\n"
        "\n"
        "msgctxt \"menu\"\n"
        "msgid \"File\"\n"
        "msgstr \"\"\n"
        "\n"
        "#~ msgid \"Old\"\n"
        "#~ msgstr \"古い\"\n";
    mnd_po_catalog_t catalog;

    /* 補助関数で読み込んで書き出すと元に戻る */
    TEST_READ_PO(po, &catalog);
    NAOS_CHECK_UINT_EQ(catalog.count, 4);
    TEST_CHECK_WRITTEN(&catalog, po);

    mnd_po_catalog_free(&catalog);
}

NAOS_SUITE(merge,
    NAOS_ADD_TEST(merge_scaffold_failure_leaves_output_empty_and_stats_zero),
    NAOS_ADD_TEST(merge_scaffold_test_util_round_trips_po)
)
