#ifndef MONDE_CLI_CLI_H
#define MONDE_CLI_CLI_H

/*
 * monde の CLI の入口。引数を解析してサブコマンドのハンドラに振り分け、終了コードを返す。
 * 標準入出力・バージョン・環境変数・時計は env で受け取り、テストで差し替えられるようにする
 */

#include <stdio.h>
#include <time.h>

typedef const char *(*mnd_cli_getenv_fn)(const char *name);  /* NULL なら getenv */
typedef time_t (*mnd_cli_now_fn)(void);                        /* NULL なら time(NULL) */

typedef struct mnd_cli_env
{
    FILE *in;                   /* 標準入力として使うストリーム（バイナリモード） */
    FILE *out;                  /* 標準出力 */
    FILE *err;                  /* 標準エラー出力 */
    const char *version;        /* --version に表示するバージョン文字列 */
    mnd_cli_getenv_fn getenv;   /* 環境変数の参照 */
    mnd_cli_now_fn now;         /* 現在時刻 */
} mnd_cli_env_t;

#define MND_CLI_EXIT_OK 0
#define MND_CLI_EXIT_FAILURE 1
#define MND_CLI_EXIT_USAGE 2

/*
 * argv[0] はプログラム名。戻り値はプロセスの終了コード（MND_CLI_EXIT_*）。
 * env と env->in / out / err は NULL でない。プロセスを終了しない
 */
int mnd_cli_run(const mnd_cli_env_t *env, int argc, char *const *argv);

#endif
