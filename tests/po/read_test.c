#include <naos/naos.h>

#include "foundation/diag.h"
#include "po/po_read.h"

NAOS_TEST(read_output_catalog_starts_empty)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_UINT_EQ(0, catalog.count);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(read,
    NAOS_ADD_TEST(read_output_catalog_starts_empty)
)
