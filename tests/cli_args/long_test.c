#include <naos/naos.h>

#include "cli_args/args.h"
#include "test_decl.h"

/* 雛形: 長いオプションのテストは後続のタスクで置き換える */
NAOS_TEST(build_declares_long_names)
{
    NAOS_REQUIRE_UINT_EQ(5, test_tool_build->option_count);
    NAOS_CHECK_STR_EQ("output", test_tool_build->options[0].long_name);
    NAOS_CHECK_STR_EQ("keys", test_tool_build->options[1].long_name);
    NAOS_CHECK_STR_EQ("level", test_tool_build->options[2].long_name);
    NAOS_CHECK_STR_EQ("verbose", test_tool_build->options[3].long_name);
    NAOS_CHECK_STR_EQ("quiet", test_tool_build->options[4].long_name);
}

NAOS_SUITE(long,
    NAOS_ADD_TEST(build_declares_long_names)
)
