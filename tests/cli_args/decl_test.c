#include <naos/naos.h>

#include "cli_args/args.h"
#include "test_decl.h"

/* 雛形: 宣言の検査のテストは後続のタスクで置き換える */
NAOS_TEST(tool_declares_three_commands)
{
    NAOS_REQUIRE_UINT_EQ(3, test_tool_program.command_count);
    NAOS_CHECK_STR_EQ("build", test_tool_program.commands[0].name);
    NAOS_CHECK_STR_EQ("copy", test_tool_program.commands[1].name);
    NAOS_CHECK_STR_EQ("cat", test_tool_program.commands[2].name);
}

NAOS_SUITE(decl,
    NAOS_ADD_TEST(tool_declares_three_commands)
)
