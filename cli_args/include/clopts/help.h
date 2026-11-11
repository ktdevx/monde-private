#ifndef MONDE_CLI_ARGS_ARGS_HELP_H
#define MONDE_CLI_ARGS_ARGS_HELP_H

#include "clopts/clopts.h"

/*
 * 解析結果の経路と宣言から、ヘルプ、バージョン、使い方の案内、経路の文字列を生成する。
 * result は mnd_args_parse が MND_ARGS_DECL_ERROR、MND_ARGS_OUT_OF_MEMORY 以外を返したものであること。
 * 成功なら確保した NUL 終端の文字列を返し、呼び出し側が mnd_args_text_free で解放する。
 * 失敗はメモリ不足だけで、NULL を返す
 */

/* 経路の最後のコマンドのヘルプ */
char *mnd_args_format_help(const mnd_args_result_t *result);

/* "<program> <version>\n" */
char *mnd_args_format_version(const mnd_args_program_t *program);

/* "Run '<path> --help' for usage.\n" */
char *mnd_args_format_usage_hint(const mnd_args_result_t *result);

/* 経路の文字列 "<path>"（改行なし）。エラーのメッセージの接頭辞 "<path>: " と同じ形 */
char *mnd_args_format_path(const mnd_args_result_t *result);

/* 上の 4 つの関数の戻り値を解放する。NULL を渡してもよい */
void mnd_args_text_free(char *text);

#endif
