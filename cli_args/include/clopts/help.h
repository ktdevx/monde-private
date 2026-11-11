#ifndef CLOPTS_HELP_H
#define CLOPTS_HELP_H

#include "clopts/clopts.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 解析結果の経路と宣言から、ヘルプ、バージョン、使い方の案内、経路の文字列を生成する。
 * result は clopts_parse が CLOPTS_DECL_ERROR、CLOPTS_OUT_OF_MEMORY 以外を返したものであること。
 * 成功なら確保した NUL 終端の文字列を返し、呼び出し側が clopts_text_free で解放する。
 * 失敗はメモリ不足だけで、NULL を返す
 */

/* 経路の最後のコマンドのヘルプ */
char *clopts_format_help(const clopts_result_t *result);

/* "<program> <version>\n" */
char *clopts_format_version(const clopts_program_t *program);

/* "Run '<path> --help' for usage.\n" */
char *clopts_format_usage_hint(const clopts_result_t *result);

/* 経路の文字列 "<path>"（改行なし）。エラーのメッセージの接頭辞 "<path>: " と同じ形 */
char *clopts_format_path(const clopts_result_t *result);

/* 上の 4 つの関数の戻り値を解放する。NULL を渡してもよい */
void clopts_text_free(char *text);

#ifdef __cplusplus
}
#endif

#endif
