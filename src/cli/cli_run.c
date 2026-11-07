/* Run: 引数の解析結果の分岐、ヘルプ・バージョン・使い方の表示、ハンドラの呼び出し、終了コード */

#include "cli/cli.h"
#include "cli/cli_commands.h"
#include "cli/cli_internal.h"
#include "cli/cli_report.h"

#include <stdio.h>

/* 仮の実装: 診断を 1 つ err に書いて失敗を返す（分岐は 3.1 で作る） */
int mnd_cli_run(const mnd_cli_env_t *env, int argc, char *const *argv)
{
    (void)argc;
    (void)argv;
    fputs("monde: error: the command line interface is not implemented\n", env->err);
    return MND_CLI_EXIT_FAILURE;
}
