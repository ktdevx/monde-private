#ifndef MONDE_CLI_ARGS_ARGS_INTERNAL_H
#define MONDE_CLI_ARGS_ARGS_INTERNAL_H

/* モジュール内部の宣言。src/cli_args/ の .c と tests/cli_args/ だけが include する */

#include "cli_args/args.h"
#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>

/* DeclCheck（args_decl.c） */

/* 宣言が妥当なら true。誤りごとに診断を追加して false を返す */
bool mnd_args_decl_check(const mnd_args_program_t *program, mnd_diag_list_t *diags);
const mnd_args_command_t *mnd_args_find_command(const mnd_args_program_t *program,
                                                const char *name, size_t name_len);
/* 見つからなければ (size_t)-1 */
size_t mnd_args_find_long(const mnd_args_command_t *command, const char *name, size_t name_len);
size_t mnd_args_find_short(const mnd_args_command_t *command, char short_name);
/* option->value_name が NULL なら "VALUE" */
const char *mnd_args_value_name(const mnd_args_option_t *option);
/* min(index, positional_count - 1) 番目の名前 */
const char *mnd_args_positional_name(const mnd_args_command_t *command, size_t index);

/* ResultStore（args_result.c）。Parser だけが使う */

bool mnd_args_result_begin(mnd_args_result_t *result, const mnd_args_command_t *command); /* values を確保 */
void mnd_args_result_set_flag(mnd_args_result_t *result, size_t option_index);
void mnd_args_result_set_value(mnd_args_result_t *result, size_t option_index, const char *value);
/* value を ',' で区切って追加する。空の項目があれば何も追加せず MND_ARGS_USAGE_ERROR、確保の失敗は MND_ARGS_OUT_OF_MEMORY */
mnd_args_status_t mnd_args_result_append_list(mnd_args_result_t *result, size_t option_index,
                                              const char *value, bool explicit_set);
bool mnd_args_result_add_positional(mnd_args_result_t *result, const char *arg);
/* 明示的に指定されていないオプションに既定値を適用する */
mnd_args_status_t mnd_args_result_apply_defaults(mnd_args_result_t *result);

#endif
