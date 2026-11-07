/* スイート run: ヘルプ、バージョン、引数なし、使い方のエラー、終了コード（1.x、9.5〜9.9） */

#include <naos/naos.h>

#include "cli/cli.h"
#include "test_util.h"

/* 雛形: 終了コードの定数が 0 / 1 / 2 であり、作業ディレクトリのパスが渡されている */
NAOS_TEST(run_scaffold_exit_codes_and_work_dir)
{
    NAOS_CHECK_INT_EQ(0, MND_CLI_EXIT_OK);
    NAOS_CHECK_INT_EQ(1, MND_CLI_EXIT_FAILURE);
    NAOS_CHECK_INT_EQ(2, MND_CLI_EXIT_USAGE);
    NAOS_CHECK_STR_NE("", test_work_dir());
}

NAOS_SUITE(run,
    NAOS_ADD_TEST(run_scaffold_exit_codes_and_work_dir)
)
