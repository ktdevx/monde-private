#include <naos/naos.h>

#include "cli_args/args.h"
#include "test_decl.h"

/* 雛形: リスト値・繰り返し・既定値のテストは後続のタスクで置き換える */
NAOS_TEST(build_declares_kinds_and_defaults)
{
    NAOS_REQUIRE_UINT_EQ(5, test_tool_build->option_count);
    NAOS_CHECK_TRUE(test_tool_build->options[0].kind == MND_ARGS_VALUE);
    NAOS_CHECK_TRUE(test_tool_build->options[0].required);
    NAOS_CHECK_TRUE(test_tool_build->options[1].kind == MND_ARGS_LIST);
    NAOS_CHECK_STR_EQ("a,b", test_tool_build->options[1].default_value);
    NAOS_CHECK_TRUE(test_tool_build->options[2].kind == MND_ARGS_VALUE);
    NAOS_CHECK_STR_EQ("1", test_tool_build->options[2].default_value);
    NAOS_CHECK_TRUE(test_tool_build->options[3].kind == MND_ARGS_FLAG);
    NAOS_CHECK_TRUE(test_tool_build->options[4].kind == MND_ARGS_FLAG);
}

NAOS_SUITE(values,
    NAOS_ADD_TEST(build_declares_kinds_and_defaults)
)
