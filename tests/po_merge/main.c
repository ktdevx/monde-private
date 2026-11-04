#include <naos/naos.h>

NAOS_DECLARE_SUITE(similarity);
NAOS_DECLARE_SUITE(index);
NAOS_DECLARE_SUITE(entry);
NAOS_DECLARE_SUITE(fuzzy);
NAOS_DECLARE_SUITE(header);
NAOS_DECLARE_SUITE(merge);
NAOS_DECLARE_SUITE(golden);

NAOS_MAIN(
    NAOS_ADD_SUITE(similarity),
    NAOS_ADD_SUITE(index),
    NAOS_ADD_SUITE(entry),
    NAOS_ADD_SUITE(fuzzy),
    NAOS_ADD_SUITE(header),
    NAOS_ADD_SUITE(merge),
    NAOS_ADD_SUITE(golden)
)
