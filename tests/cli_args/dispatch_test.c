#include <naos/naos.h>

#include "cli_args/args.h"
#include "test_decl.h"

/* 雛形: サブコマンドの振り分けのテストは後続のタスクで置き換える */
NAOS_TEST(tool_has_name_and_version)
{
    NAOS_CHECK_STR_EQ("tool", test_tool_program.name);
    NAOS_CHECK_STR_EQ("1.2.3", test_tool_program.version);
}

NAOS_SUITE(dispatch,
    NAOS_ADD_TEST(tool_has_name_and_version)
)
