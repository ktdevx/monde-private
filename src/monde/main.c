/*
 * 実行ファイル monde の main: UTF-8 の引数列を取得し、標準入出力を組み込んだ環境で
 * CLI の入口を呼び、戻り値をそのままプロセスの終了コードにする
 */

#include "cli/cli.h"
#include "cli_args/args_argv.h"

#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* 引数列を取得できなかったときの診断を `monde: error: <message>` の形で書く */
static void report_argv_error(const mnd_diag_list_t *diags)
{
    size_t count = mnd_diag_list_count(diags);
    size_t i;

    for (i = 0; i < count; i++)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);

        if ((diag != NULL) && (diag->message != NULL))
        {
            fprintf(stderr, "monde: error: %s\n", diag->message);
        }
    }
    if ((count == 0) || diags->dropped)
    {
        /* 診断を記録できなかった場合も、失敗したことは必ず伝える */
        fputs("monde: error: cannot read the command line\n", stderr);
    }
    fflush(stderr);
}

int main(int argc, char **argv)
{
    mnd_args_argv_t args = MND_ARGS_ARGV_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_cli_env_t env = { NULL, NULL, NULL, MONDE_VERSION, NULL, NULL };
    int status;

#ifdef _WIN32
    /* 改行コードを変換させないため、標準入出力をバイナリモードにする（3.8） */
    (void)_setmode(_fileno(stdin), _O_BINARY);
    (void)_setmode(_fileno(stdout), _O_BINARY);
    (void)_setmode(_fileno(stderr), _O_BINARY);
#endif

    /* Windows では UTF-16 のコマンドラインから UTF-8 に変換した引数列を得る */
    if (!mnd_args_argv_get(argc, argv, &args, &diags))
    {
        report_argv_error(&diags);
        mnd_diag_list_free(&diags);
        mnd_args_argv_free(&args);
        return MND_CLI_EXIT_FAILURE;
    }
    mnd_diag_list_free(&diags);

    /* getenv と now は NULL（実際の環境変数と現在時刻を使う） */
    env.in = stdin;
    env.out = stdout;
    env.err = stderr;
    status = mnd_cli_run(&env, args.argc, args.argv);

    mnd_args_argv_free(&args);
    return status;
}
