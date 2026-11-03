#include <naos/naos.h>

#include "po/po_write.h"

NAOS_TEST(write_options_init_is_wrap_and_lf)
{
    mnd_po_write_options_t options = MND_PO_WRITE_OPTIONS_INIT;

    NAOS_CHECK_FALSE(options.no_wrap);
    NAOS_CHECK_INT_EQ(MND_EOL_LF, options.eol);
}

NAOS_SUITE(write,
    NAOS_ADD_TEST(write_options_init_is_wrap_and_lf)
)
