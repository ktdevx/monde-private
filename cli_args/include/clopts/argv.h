#ifndef MONDE_CLI_ARGS_ARGS_ARGV_H
#define MONDE_CLI_ARGS_ARGS_ARGV_H

#include "clopts/errors.h"

#include <stdbool.h>

/* プロセスのコマンドライン引数を UTF-8 の文字列の列として取得する */

typedef struct mnd_args_argv
{
    int argc;
    char **argv;   /* argv[argc] == NULL */
    bool owned;    /* argv とその文字列を本構造体が所有する（Windows） */
} mnd_args_argv_t;

#define MND_ARGS_ARGV_INIT { 0, NULL, false }

/* 失敗（メモリ不足、不正な UTF-16、コマンドラインを読めない）なら errors にエラーを追加して false を返す */
bool mnd_args_argv_get(int argc, char **argv, mnd_args_argv_t *out, clopts_errors_t *errors);

/* owned なら解放する。初期状態に戻す。初期状態・解放済みに対しても安全 */
void mnd_args_argv_free(mnd_args_argv_t *args);

#ifdef _WIN32
#include <wchar.h>

/* UTF-16 の引数列を UTF-8 に変換する。mnd_args_argv_get の内部処理で、テストのために公開する */
bool mnd_args_argv_from_wide(int argc, wchar_t *const *wargv, mnd_args_argv_t *out,
                             clopts_errors_t *errors);
#endif

#endif
