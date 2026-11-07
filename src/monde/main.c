/* 実行ファイル monde の main: 環境を組み立てて CLI の入口を呼び、戻り値を終了コードにする */

#include "cli/cli.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    /* 仮の環境（引数列の UTF-8 化と Windows のバイナリモードの設定は 3.2 で加える） */
    mnd_cli_env_t env = { NULL, NULL, NULL, MONDE_VERSION, NULL, NULL };

    env.in = stdin;
    env.out = stdout;
    env.err = stderr;
    return mnd_cli_run(&env, argc, argv);
}
