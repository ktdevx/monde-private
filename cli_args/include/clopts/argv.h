#ifndef CLOPTS_ARGV_H
#define CLOPTS_ARGV_H

#include "clopts/errors.h"

#include <stdbool.h>

#ifdef _WIN32
#include <wchar.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* プロセスのコマンドライン引数を UTF-8 の文字列の列として取得する */

typedef struct clopts_argv
{
    int argc;
    char **argv;   /* argv[argc] == NULL */
    bool owned;    /* argv とその文字列を本構造体が所有する（Windows） */
} clopts_argv_t;

#define CLOPTS_ARGV_INIT { 0, NULL, false }

/* 失敗（メモリ不足、不正な UTF-16、コマンドラインを読めない）なら errors にエラーを追加して false を返す */
bool clopts_argv_get(int argc, char **argv, clopts_argv_t *out, clopts_errors_t *errors);

/* owned なら解放する。初期状態に戻す。初期状態・解放済みに対しても安全 */
void clopts_argv_free(clopts_argv_t *args);

#ifdef _WIN32
/* UTF-16 の引数列を UTF-8 に変換する。clopts_argv_get の内部処理で、テストのために公開する */
bool clopts_argv_from_wide(int argc, wchar_t *const *wargv, clopts_argv_t *out,
                           clopts_errors_t *errors);
#endif

#ifdef __cplusplus
}
#endif

#endif
