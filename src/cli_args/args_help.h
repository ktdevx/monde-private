#ifndef MONDE_CLI_ARGS_ARGS_HELP_H
#define MONDE_CLI_ARGS_ARGS_HELP_H

#include "cli_args/args.h"
#include "foundation/buf.h"

#include <stdbool.h>

/*
 * 解析結果の経路と宣言から、ヘルプ、バージョン、使い方の案内、経路の文字列を生成する。
 * result は mnd_args_parse が MND_ARGS_DECL_ERROR、MND_ARGS_OUT_OF_MEMORY 以外を返したものであること。
 * 出力は out の末尾に追加する。失敗はメモリ不足だけで、false を返す
 */

/* 経路の最後のコマンドのヘルプ */
bool mnd_args_format_help(const mnd_args_result_t *result, mnd_buf_t *out);

/* "<program> <version>\n" */
bool mnd_args_format_version(const mnd_args_program_t *program, mnd_buf_t *out);

/* "Run '<path> --help' for usage.\n" */
bool mnd_args_format_usage_hint(const mnd_args_result_t *result, mnd_buf_t *out);

/* 経路の文字列 "<path>"（改行なし）。診断の接頭辞 "<path>: " と同じ形 */
bool mnd_args_format_path(const mnd_args_result_t *result, mnd_buf_t *out);

#endif
