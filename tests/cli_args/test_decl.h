#ifndef MONDE_TESTS_CLI_ARGS_TEST_DECL_H
#define MONDE_TESTS_CLI_ARGS_TEST_DECL_H

#include "cli_args/args.h"

/*
 * テスト専用の架空のプログラム "tool"（バージョン 1.2.3）の宣言。
 *   build: --output/-o <FILE>（値、必須）、--keys/-k <KEY>（リスト、既定値 "a,b"）、
 *          --level <N>（値、既定値 "1"）、--verbose/-v（フラグ）、--quiet/-q（フラグ）。
 *          位置引数 SOURCE（1 個ちょうど）
 *   copy:  オプションなし。位置引数 SRC、DEST（2 個ちょうど）
 *   cat:   オプションなし。位置引数 FILE（0 個以上、上限なし）
 */
extern const mnd_args_program_t test_tool_program;

/* test_tool_program の各サブコマンド（宣言の順） */
extern const mnd_args_command_t *const test_tool_build;
extern const mnd_args_command_t *const test_tool_copy;
extern const mnd_args_command_t *const test_tool_cat;

#endif
