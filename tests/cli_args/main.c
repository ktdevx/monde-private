#include <naos/naos.h>

NAOS_DECLARE_SUITE(decl);
NAOS_DECLARE_SUITE(dispatch);
NAOS_DECLARE_SUITE(long);
NAOS_DECLARE_SUITE(short);
NAOS_DECLARE_SUITE(values);
NAOS_DECLARE_SUITE(positional);
NAOS_DECLARE_SUITE(errors);
NAOS_DECLARE_SUITE(help);
NAOS_DECLARE_SUITE(single);
NAOS_DECLARE_SUITE(nested);
NAOS_DECLARE_SUITE(inherit);
NAOS_DECLARE_SUITE(argv);

NAOS_MAIN(
    NAOS_ADD_SUITE(decl),
    NAOS_ADD_SUITE(dispatch),
    NAOS_ADD_SUITE(long),
    NAOS_ADD_SUITE(short),
    NAOS_ADD_SUITE(values),
    NAOS_ADD_SUITE(positional),
    NAOS_ADD_SUITE(errors),
    NAOS_ADD_SUITE(help),
    NAOS_ADD_SUITE(single),
    NAOS_ADD_SUITE(nested),
    NAOS_ADD_SUITE(inherit),
    NAOS_ADD_SUITE(argv)
)
