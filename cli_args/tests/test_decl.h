#ifndef CLOPTS_TESTS_TEST_DECL_H
#define CLOPTS_TESTS_TEST_DECL_H

#include "clopts/clopts.h"

/*
 * テスト専用の架空のプログラム "tool"（バージョン 1.2.3）の宣言。ルートコマンドは
 * オプションと位置引数を持たず、次の 3 つの末端コマンドを子コマンドとして持つ（1 段）。
 *   build: --output/-o <FILE>（値、必須）、--keys/-k <KEY>（リスト、既定値 "a,b"）、
 *          --level <N>（値、既定値 "1"）、--verbose/-v（フラグ）、--quiet/-q（フラグ）。
 *          位置引数 SOURCE（1 個ちょうど）
 *   copy:  オプションなし。位置引数 SRC、DEST（2 個ちょうど）
 *   cat:   オプションなし。位置引数 FILE（0 個以上、上限なし）
 */
extern const clopts_program_t test_tool_program;

/* test_tool_program のルートコマンドの各子コマンド（宣言の順） */
extern const clopts_command_t *const test_tool_build;
extern const clopts_command_t *const test_tool_copy;
extern const clopts_command_t *const test_tool_cat;

/*
 * テスト専用の架空のプログラム "single"（バージョン 2.0）の宣言。サブコマンドを持たず、
 * ルートコマンドが末端コマンドである。
 *   --output/-o <FILE>（値）、--verbose/-v（フラグ）、--keys/-k <KEY>（リスト）。
 *   位置引数 FILE（0 個以上、上限なし）
 */
extern const clopts_program_t test_single_program;

/*
 * テスト専用の架空のプログラム "nest"（バージョン 3.0）の宣言。入れ子の構成で、
 * 子孫に引き継ぐオプションを持つ。
 *   ルート: --verbose/-v（フラグ、引き継ぐ）、--config/-c <FILE>（値、引き継ぐ、既定値 "nest.cfg"）、
 *           --dry-run（フラグ、引き継がない）
 *   remote（子を持つ）: --token/-t <TOKEN>（値、引き継ぐ、必須）、--tags <TAG>（リスト、引き継ぐ）
 *     remote add:    --force/-f（フラグ）。位置引数 NAME、URL（2 個ちょうど）
 *     remote remove: オプションなし。位置引数 NAME（1 個ちょうど）
 *   status: オプションなし。位置引数なし
 */
extern const clopts_program_t test_nest_program;

/* test_nest_program の各コマンド */
extern const clopts_command_t *const test_nest_remote;
extern const clopts_command_t *const test_nest_remote_add;
extern const clopts_command_t *const test_nest_remote_remove;
extern const clopts_command_t *const test_nest_status;

#endif
