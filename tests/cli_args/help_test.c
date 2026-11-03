#include <naos/naos.h>

#include "cli_args/args_help.h"
#include "test_decl.h"

/* 雛形: 文面のテストは後続のタスクで置き換える */
NAOS_TEST(commands_have_summaries)
{
    size_t i;

    NAOS_CHECK_STR_EQ("A sample tool for testing.", test_tool_program.description);
    for (i = 0; i < test_tool_program.command_count; i++)
    {
        NAOS_CHECK_NOT_NULL(test_tool_program.commands[i].summary);
    }
}

NAOS_SUITE(help,
    NAOS_ADD_TEST(commands_have_summaries)
)
