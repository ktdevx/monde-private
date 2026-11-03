#include <naos/naos.h>

#include "po/po_internal.h"

NAOS_TEST(escape_unescape_status_ok_is_distinct)
{
    NAOS_CHECK_INT_NE(MND_PO_UNESCAPE_OK, MND_PO_UNESCAPE_BAD_ESCAPE);
    NAOS_CHECK_INT_NE(MND_PO_UNESCAPE_OK, MND_PO_UNESCAPE_NUL);
    NAOS_CHECK_INT_NE(MND_PO_UNESCAPE_OK, MND_PO_UNESCAPE_BAD_UTF8);
    NAOS_CHECK_INT_NE(MND_PO_UNESCAPE_OK, MND_PO_UNESCAPE_NO_MEMORY);
}

NAOS_SUITE(escape,
    NAOS_ADD_TEST(escape_unescape_status_ok_is_distinct)
)
