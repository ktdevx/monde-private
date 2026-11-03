#include <naos/naos.h>

NAOS_DECLARE_SUITE(buf);
NAOS_DECLARE_SUITE(diag);
NAOS_DECLARE_SUITE(utf8);
NAOS_DECLARE_SUITE(eol);
NAOS_DECLARE_SUITE(source);
NAOS_DECLARE_SUITE(file_io);

/*
 * wide は Windows でだけビルドする。マクロの引数の中に #ifdef を書けないため、
 * NAOS_MAIN を OS ごとに書き分ける
 */
#ifdef _WIN32
NAOS_DECLARE_SUITE(wide);

NAOS_MAIN(
    NAOS_ADD_SUITE(buf),
    NAOS_ADD_SUITE(diag),
    NAOS_ADD_SUITE(utf8),
    NAOS_ADD_SUITE(eol),
    NAOS_ADD_SUITE(source),
    NAOS_ADD_SUITE(file_io),
    NAOS_ADD_SUITE(wide)
)
#else
NAOS_MAIN(
    NAOS_ADD_SUITE(buf),
    NAOS_ADD_SUITE(diag),
    NAOS_ADD_SUITE(utf8),
    NAOS_ADD_SUITE(eol),
    NAOS_ADD_SUITE(source),
    NAOS_ADD_SUITE(file_io)
)
#endif
