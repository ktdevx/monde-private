#include <naos/naos.h>

#include "foundation/buf.h"

NAOS_TEST(init_macro_is_empty)
{
    mnd_buf buf = MND_BUF_INIT;

    NAOS_CHECK_NULL(buf.data);
    NAOS_CHECK_UINT_EQ(0, buf.len);
    NAOS_CHECK_UINT_EQ(0, buf.cap);
}

NAOS_SUITE(buf,
    NAOS_ADD_TEST(init_macro_is_empty)
)
