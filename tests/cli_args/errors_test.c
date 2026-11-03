#include <naos/naos.h>

#include "cli_args/args.h"
#include "test_decl.h"

/* 雛形: 誤りの扱いのテストは後続のタスクで置き換える */
NAOS_TEST(result_init_macro_is_no_command)
{
    mnd_args_result_t result = MND_ARGS_RESULT_INIT;

    NAOS_CHECK_TRUE(result.status == MND_ARGS_NO_COMMAND);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_NULL(result.values);
    NAOS_CHECK_NULL(result.positionals);
    NAOS_CHECK_UINT_EQ(0, result.positional_count);
    NAOS_CHECK_UINT_EQ(0, result.positional_cap);
}

NAOS_SUITE(errors,
    NAOS_ADD_TEST(result_init_macro_is_no_command)
)
