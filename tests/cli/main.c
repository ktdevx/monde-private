#include <naos/naos.h>

NAOS_DECLARE_SUITE(run);
NAOS_DECLARE_SUITE(io);
NAOS_DECLARE_SUITE(report);
NAOS_DECLARE_SUITE(time);
NAOS_DECLARE_SUITE(language);
NAOS_DECLARE_SUITE(extract);
NAOS_DECLARE_SUITE(init);
NAOS_DECLARE_SUITE(update);
NAOS_DECLARE_SUITE(translate);
NAOS_DECLARE_SUITE(roundtrip);

NAOS_MAIN(
    NAOS_ADD_SUITE(run),
    NAOS_ADD_SUITE(io),
    NAOS_ADD_SUITE(report),
    NAOS_ADD_SUITE(time),
    NAOS_ADD_SUITE(language),
    NAOS_ADD_SUITE(extract),
    NAOS_ADD_SUITE(init),
    NAOS_ADD_SUITE(update),
    NAOS_ADD_SUITE(translate),
    NAOS_ADD_SUITE(roundtrip)
)
