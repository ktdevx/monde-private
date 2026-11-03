#include <naos/naos.h>

#include "cli_args/args.h"
#include "test_decl.h"

/* 雛形: 短いオプションのテストは後続のタスクで置き換える */
NAOS_TEST(build_declares_short_names)
{
    NAOS_REQUIRE_UINT_EQ(5, test_tool_build->option_count);
    NAOS_CHECK_INT_EQ('o', test_tool_build->options[0].short_name);
    NAOS_CHECK_INT_EQ('k', test_tool_build->options[1].short_name);
    NAOS_CHECK_INT_EQ('\0', test_tool_build->options[2].short_name);
    NAOS_CHECK_INT_EQ('v', test_tool_build->options[3].short_name);
    NAOS_CHECK_INT_EQ('q', test_tool_build->options[4].short_name);
}

NAOS_SUITE(short,
    NAOS_ADD_TEST(build_declares_short_names)
)
