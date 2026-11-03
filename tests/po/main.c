#include <naos/naos.h>

NAOS_DECLARE_SUITE(entry);
NAOS_DECLARE_SUITE(header);
NAOS_DECLARE_SUITE(escape);
NAOS_DECLARE_SUITE(read);
NAOS_DECLARE_SUITE(read_error);
NAOS_DECLARE_SUITE(linebreak);
NAOS_DECLARE_SUITE(write);
NAOS_DECLARE_SUITE(roundtrip);
NAOS_DECLARE_SUITE(init);

NAOS_MAIN(
    NAOS_ADD_SUITE(entry),
    NAOS_ADD_SUITE(header),
    NAOS_ADD_SUITE(escape),
    NAOS_ADD_SUITE(read),
    NAOS_ADD_SUITE(read_error),
    NAOS_ADD_SUITE(linebreak),
    NAOS_ADD_SUITE(write),
    NAOS_ADD_SUITE(roundtrip),
    NAOS_ADD_SUITE(init)
)
