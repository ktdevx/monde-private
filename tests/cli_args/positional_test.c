#include <naos/naos.h>

#include "cli_args/args.h"
#include "test_decl.h"

/* 雛形: 位置引数のテストは後続のタスクで置き換える */
NAOS_TEST(commands_declare_positional_ranges)
{
    NAOS_CHECK_UINT_EQ(1, test_tool_build->min_positionals);
    NAOS_CHECK_UINT_EQ(1, test_tool_build->max_positionals);
    NAOS_CHECK_UINT_EQ(2, test_tool_copy->min_positionals);
    NAOS_CHECK_UINT_EQ(2, test_tool_copy->max_positionals);
    NAOS_CHECK_UINT_EQ(0, test_tool_cat->min_positionals);
    NAOS_CHECK_UINT_EQ(MND_ARGS_UNLIMITED, test_tool_cat->max_positionals);
}

NAOS_SUITE(positional,
    NAOS_ADD_TEST(commands_declare_positional_ranges)
)
