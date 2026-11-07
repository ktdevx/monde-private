#include <naos/naos.h>

NAOS_DECLARE_SUITE(markdown);
NAOS_DECLARE_SUITE(front_matter);
NAOS_DECLARE_SUITE(extract);
NAOS_DECLARE_SUITE(splice);
NAOS_DECLARE_SUITE(check);
NAOS_DECLARE_SUITE(translate);
NAOS_DECLARE_SUITE(roundtrip);

NAOS_MAIN(
    NAOS_ADD_SUITE(markdown),
    NAOS_ADD_SUITE(front_matter),
    NAOS_ADD_SUITE(extract),
    NAOS_ADD_SUITE(splice),
    NAOS_ADD_SUITE(check),
    NAOS_ADD_SUITE(translate),
    NAOS_ADD_SUITE(roundtrip)
)
