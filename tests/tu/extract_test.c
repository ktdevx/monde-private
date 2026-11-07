/* スイート extract: 抽出の規則（3.1〜3.7、3.9） */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "po/po.h"
#include "tu/tu.h"

#include <stddef.h>

/* セグメントのない文書からはエントリを加えない */
NAOS_TEST(extract_empty_document_adds_no_entries)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_po_catalog_t catalog;
    mnd_tu_doc_t *doc = NULL;

    mnd_po_catalog_init(&catalog);
    NAOS_REQUIRE_TRUE(mnd_tu_scan(NULL, 0, NULL, &doc, &diags));
    (void)mnd_tu_extract(doc, "doc.md", &catalog, &diags);
    NAOS_CHECK_UINT_EQ(0, catalog.count);
    mnd_po_catalog_free(&catalog);
    mnd_tu_doc_free(doc);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(extract,
    NAOS_ADD_TEST(extract_empty_document_adds_no_entries)
)
