#include <naos/naos.h>

#include "cli_args/args_argv.h"

/* 雛形: 引数の取得のテストは後続のタスクで置き換える */
NAOS_TEST(argv_init_macro_is_empty)
{
    mnd_args_argv_t args = MND_ARGS_ARGV_INIT;

    NAOS_CHECK_INT_EQ(0, args.argc);
    NAOS_CHECK_NULL(args.argv);
    NAOS_CHECK_FALSE(args.owned);
}

NAOS_SUITE(argv,
    NAOS_ADD_TEST(argv_init_macro_is_empty)
)
