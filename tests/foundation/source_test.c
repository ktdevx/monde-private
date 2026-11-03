#include <naos/naos.h>

#include "foundation/source.h"

NAOS_TEST(span_holds_offsets)
{
    mnd_span span = { 2, 5 };

    NAOS_CHECK_UINT_EQ(2, span.start);
    NAOS_CHECK_UINT_EQ(5, span.end);
}

NAOS_SUITE(source,
    NAOS_ADD_TEST(span_holds_offsets)
)
