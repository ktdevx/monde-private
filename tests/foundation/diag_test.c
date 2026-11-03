#include <naos/naos.h>

#include "foundation/diag.h"

NAOS_TEST(list_init_macro_is_empty)
{
    mnd_diag_list list = MND_DIAG_LIST_INIT;

    NAOS_CHECK_NULL(list.items);
    NAOS_CHECK_UINT_EQ(0, list.count);
    NAOS_CHECK_UINT_EQ(0, list.cap);
    NAOS_CHECK_FALSE(list.dropped);
}

NAOS_SUITE(diag,
    NAOS_ADD_TEST(list_init_macro_is_empty)
)
