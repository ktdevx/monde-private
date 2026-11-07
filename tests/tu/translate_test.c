/* スイート translate: 再構成（Rebuilder）（4.1〜4.7、6.1〜6.4、7.4、7.7） */

#include <naos/naos.h>

#include "foundation/eol.h"
#include "tu/tu.h"

#include <stddef.h>

/* 再構成のオプションの既定値は fuzzy 不採用・LF・パスなしで、統計の既定値はすべて 0 */
NAOS_TEST(translate_options_and_stats_init)
{
    mnd_tu_translate_options_t options = MND_TU_TRANSLATE_OPTIONS_INIT;
    mnd_tu_stats_t stats = MND_TU_STATS_INIT;

    NAOS_CHECK_FALSE(options.use_fuzzy);
    NAOS_CHECK_INT_EQ((int)MND_EOL_LF, (int)options.eol);
    NAOS_CHECK_TRUE(options.catalog_path == NULL);
    NAOS_CHECK_UINT_EQ(0, stats.total);
    NAOS_CHECK_UINT_EQ(0, stats.translated);
    NAOS_CHECK_UINT_EQ(0, stats.fuzzy);
    NAOS_CHECK_UINT_EQ(0, stats.untranslated);
    NAOS_CHECK_UINT_EQ(0, stats.missing);
}

NAOS_SUITE(translate,
    NAOS_ADD_TEST(translate_options_and_stats_init)
)
