/* Parse: 解析の入口、開始位置、空の内容、NUL（1.x） */

#include <naos/naos.h>

#include "markdown/md_parse.h"

NAOS_TEST(parse_options_init_is_all_defaults)
{
    mnd_md_options_t options = MND_MD_OPTIONS_INIT;

    NAOS_CHECK_NULL(options.dialect);
    NAOS_CHECK_UINT_EQ(options.body_start, (size_t)0);
    NAOS_CHECK_NULL(options.path);
    NAOS_CHECK_NULL(options.allocator);
}

NAOS_SUITE(parse,
    NAOS_ADD_TEST(parse_options_init_is_all_defaults)
)
