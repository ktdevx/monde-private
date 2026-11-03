#include <naos/naos.h>

#include "foundation/diag.h"
#include "po/po_read.h"

NAOS_TEST(read_error_diag_list_starts_without_error)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_FALSE(mnd_diag_list_has_error(&diags));
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(read_error,
    NAOS_ADD_TEST(read_error_diag_list_starts_without_error)
)
