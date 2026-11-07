#include <naos/naos.h>

NAOS_DECLARE_SUITE(front_matter);
NAOS_DECLARE_SUITE(lines);
NAOS_DECLARE_SUITE(scalar);
NAOS_DECLARE_SUITE(block);
NAOS_DECLARE_SUITE(flow);
NAOS_DECLARE_SUITE(model);
NAOS_DECLARE_SUITE(path);
NAOS_DECLARE_SUITE(format);
NAOS_DECLARE_SUITE(error);
NAOS_DECLARE_SUITE(roundtrip);

NAOS_MAIN(
    NAOS_ADD_SUITE(front_matter),
    NAOS_ADD_SUITE(lines),
    NAOS_ADD_SUITE(scalar),
    NAOS_ADD_SUITE(block),
    NAOS_ADD_SUITE(flow),
    NAOS_ADD_SUITE(model),
    NAOS_ADD_SUITE(path),
    NAOS_ADD_SUITE(format),
    NAOS_ADD_SUITE(error),
    NAOS_ADD_SUITE(roundtrip)
)
