#include <naos/naos.h>

#include "foundation/eol.h"

NAOS_TEST(eol_kinds_are_distinct)
{
    NAOS_CHECK_INT_NE(MND_EOL_NONE, MND_EOL_LF);
    NAOS_CHECK_INT_NE(MND_EOL_NONE, MND_EOL_CRLF);
    NAOS_CHECK_INT_NE(MND_EOL_LF, MND_EOL_CRLF);
}

NAOS_SUITE(eol,
    NAOS_ADD_TEST(eol_kinds_are_distinct)
)
