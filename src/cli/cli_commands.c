/* Commands: プログラムとサブコマンドの宣言のテーブルと、サブコマンド名からハンドラへの対応 */

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"

#include <stddef.h>

/* 仮の実装: サブコマンドを持たない宣言を返す（宣言のテーブルは 2.1 で作る） */
mnd_args_program_t mnd_cli_program(const char *version)
{
    mnd_args_program_t program = { "monde", NULL, NULL, NULL, 0 };

    program.version = version;
    return program;
}

/* 仮の実装: どの名前にもハンドラを返さない */
mnd_cli_handler_fn mnd_cli_find_handler(const char *name)
{
    (void)name;
    return NULL;
}
