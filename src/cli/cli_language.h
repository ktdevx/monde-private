#ifndef MONDE_CLI_CLI_LANGUAGE_H
#define MONDE_CLI_CLI_LANGUAGE_H

/*
 * Language: --language と環境変数から言語コードを決め、検査する。
 * モジュール内部のヘッダ（src/cli/ と tests/cli/ だけが include する）
 */

#include "cli/cli.h"
#include "foundation/diag.h"

#include <stdbool.h>

typedef struct mnd_cli_language
{
    const char *code;      /* 決定した言語コード（確保した文字列。呼び出し側が free） */
    const char *variable;  /* 環境変数から決めたときはその名前。--language なら NULL */
} mnd_cli_language_t;

/*
 * explicit が NULL でなければそれを使う。NULL なら LC_ALL → LC_MESSAGES → LANG の順に env->getenv で引き、
 * 空・"C"・"POSIX"（"." / "@" 以降を除いた後で判定）は未設定として次を見る。
 * 値は "." / "@" 以降を除く。決まらなければ使い方のエラー（--language を求める）、
 * 値が空か空白・制御文字を含めば使い方のエラー（値と出どころを示す）。どちらも診断を追加して false
 */
bool mnd_cli_resolve_language(const mnd_cli_env_t *env, const char *explicit,
                              mnd_cli_language_t *out, mnd_diag_list_t *diags);

#endif
