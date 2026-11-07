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
 * 環境変数の値は "." / "@" 以降を除く（explicit はそのまま使う）。決まらなければ使い方のエラー（--language を求める）、
 * 決めた値が空か空白・制御文字・非 ASCII のバイトを含めば使い方のエラー（値と出どころを示す。
 * 制御文字と非 ASCII は \xHH の形）。どちらも診断を追加して false。失敗時の out は NULL / NULL
 */
bool mnd_cli_resolve_language(const mnd_cli_env_t *env, const char *explicit,
                              mnd_cli_language_t *out, mnd_diag_list_t *diags);

#endif
