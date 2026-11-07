#ifndef MONDE_CLI_CLI_COMMANDS_H
#define MONDE_CLI_CLI_COMMANDS_H

/*
 * Commands: プログラムとサブコマンドの宣言（mnd_args_program_t）、オプション名の定数、
 * サブコマンド名からハンドラへの対応。モジュール内部のヘッダ（src/cli/ と tests/cli/ だけが include する）
 */

#include "cli_args/args.h"

#include <stdbool.h>

/* ハンドラが受け取る文脈（定義は cli/cli_internal.h） */
struct mnd_cli_ctx;

/* オプションの長い名前 */
#define MND_CLI_OPT_OUTPUT "output"
#define MND_CLI_OPT_QUIET "quiet"
#define MND_CLI_OPT_FRONT_MATTER_KEYS "front-matter-keys"
#define MND_CLI_OPT_PROJECT_ID_VERSION "project-id-version"
#define MND_CLI_OPT_MSGID_BUGS_ADDRESS "msgid-bugs-address"
#define MND_CLI_OPT_NO_WRAP "no-wrap"
#define MND_CLI_OPT_LANGUAGE "language"
#define MND_CLI_OPT_NO_FUZZY_MATCHING "no-fuzzy-matching"
#define MND_CLI_OPT_USE_FUZZY "use-fuzzy"

/* サブコマンドのハンドラ。成功なら true、失敗なら診断を ctx に追加して false */
typedef bool (*mnd_cli_handler_fn)(struct mnd_cli_ctx *ctx);

/* プログラムの宣言を返す。version（--version に表示する文字列）だけを実行時に差し替える */
mnd_args_program_t mnd_cli_program(const char *version);

/* サブコマンド名に対応するハンドラ。未知の名前なら NULL */
mnd_cli_handler_fn mnd_cli_find_handler(const char *name);

#endif
