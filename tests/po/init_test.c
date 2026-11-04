#include <naos/naos.h>

#include "foundation/diag.h"
#include "po/po_init.h"

NAOS_TEST(init_template_catalog_starts_empty)
{
    mnd_po_catalog_t catalog = MND_PO_CATALOG_INIT;

    NAOS_CHECK_NULL(catalog.entries);
    NAOS_CHECK_UINT_EQ(0, catalog.count);
}

NAOS_SUITE(init,
    NAOS_ADD_TEST(init_template_catalog_starts_empty)
)
