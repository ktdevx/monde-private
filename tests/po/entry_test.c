#include <naos/naos.h>

#include "po/po.h"

NAOS_TEST(entry_catalog_init_macro_is_empty)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;

    NAOS_CHECK_NULL(catalog.entries);
    NAOS_CHECK_UINT_EQ(0, catalog.count);
    NAOS_CHECK_UINT_EQ(0, catalog.cap);
}

NAOS_SUITE(entry,
    NAOS_ADD_TEST(entry_catalog_init_macro_is_empty)
)
