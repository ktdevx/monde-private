#ifndef MONDE_CLI_ARGS_ARGS_HELP_H
#define MONDE_CLI_ARGS_ARGS_HELP_H

#include "cli_args/args.h"
#include "foundation/buf.h"

#include <stdbool.h>

/*
 * 宣言からヘルプ、バージョン、使い方の案内の文面を生成する。
 * 出力は out の末尾に追加する。失敗はメモリ不足だけで、false を返す
 */

/* command が NULL なら全体のヘルプ、そうでなければそのサブコマンドのヘルプ */
bool mnd_args_format_help(const mnd_args_program_t *program, const mnd_args_command_t *command,
                          mnd_buf_t *out);
/* "<program> <version>\n" */
bool mnd_args_format_version(const mnd_args_program_t *program, mnd_buf_t *out);
/* command が NULL なら "Run '<program> --help' for usage.\n"、そうでなければ "Run '<program> <command> --help' for usage.\n" */
bool mnd_args_format_usage_hint(const mnd_args_program_t *program, const mnd_args_command_t *command,
                                mnd_buf_t *out);

#endif
