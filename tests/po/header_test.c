#include <naos/naos.h>

#include "po/po.h"

NAOS_TEST(header_date_size_holds_po_date)
{
    /* "YYYY-MM-DD HH:MM+ZZZZ" と終端の NUL が収まる */
    NAOS_CHECK_UINT_GE(MND_PO_DATE_SIZE, sizeof("YYYY-MM-DD HH:MM+ZZZZ"));
}

NAOS_SUITE(header,
    NAOS_ADD_TEST(header_date_size_holds_po_date)
)
