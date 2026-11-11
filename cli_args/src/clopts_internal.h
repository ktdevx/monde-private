#ifndef MONDE_CLI_ARGS_ARGS_INTERNAL_H
#define MONDE_CLI_ARGS_ARGS_INTERNAL_H

/* モジュール内部の宣言。src/cli_args/ の .c と tests/cli_args/ だけが include する */

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>

/* printf 形式の書式をコンパイル時に検査させる属性（MSVC では空） */
#if defined(__MINGW32__)
#define CLOPTS_PRINTF_LIKE(fmt, args) __attribute__((format(gnu_printf, fmt, args)))
#elif defined(__GNUC__) || defined(__clang__)
#define CLOPTS_PRINTF_LIKE(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define CLOPTS_PRINTF_LIKE(fmt, args)
#endif

/* Errors（clopts_errors.c） */

/* printf 形式でメッセージを組み立てて末尾に追加する。失敗したら dropped を立て、ほかの内容は変えない */
void clopts_errors_add(clopts_errors_t *errors, const char *format, ...) CLOPTS_PRINTF_LIKE(2, 3);

/* DeclCheck（args_decl.c） */

/* 経路上の 1 つのオプションの位置。level は段の添字、index はその段のコマンドの options の添字 */
typedef struct mnd_args_ref
{
    size_t level;
    size_t index;
} mnd_args_ref_t;

/* 宣言が妥当なら true。誤りごとに診断を追加して false を返す */
bool mnd_args_decl_check(const mnd_args_program_t *program, mnd_diag_list_t *diags);

/* parent の子コマンドを名前で探す。見つからなければ NULL */
const mnd_args_command_t *mnd_args_find_child(const mnd_args_command_t *parent,
                                              const char *name, size_t name_len);

/* 有効なオプションを名前で探す。見つかれば *ref に位置を入れて true（12.1、12.2） */
bool mnd_args_find_long(const mnd_args_result_t *result, const char *name, size_t name_len,
                        mnd_args_ref_t *ref);
bool mnd_args_find_short(const mnd_args_result_t *result, char short_name, mnd_args_ref_t *ref);

/* 有効なオプションの列挙。順序は「最後の段のオプション、親の引き継ぐオプション、…、ルートの引き継ぐオプション」 */
size_t mnd_args_effective_count(const mnd_args_result_t *result);
mnd_args_ref_t mnd_args_effective_at(const mnd_args_result_t *result, size_t i); /* i < count */

/* ref が指すオプションの宣言 */
const mnd_args_option_t *mnd_args_option_at(const mnd_args_result_t *result, mnd_args_ref_t ref);

/* option->value_name が NULL なら "VALUE" */
const char *mnd_args_value_name(const mnd_args_option_t *option);

/* min(index, positional_count - 1) 番目の名前 */
const char *mnd_args_positional_name(const mnd_args_command_t *command, size_t index);

/* ResultStore（args_result.c）。Parser だけが使う */

/* command を新しい段として加え、値の配列を確保する。失敗はメモリ不足 */
bool mnd_args_result_push(mnd_args_result_t *result, const mnd_args_command_t *command);

/* 段を level_count 個まで減らし、取り除いた段の値を解放する（help の語の誤りで使う） */
void mnd_args_result_truncate(mnd_args_result_t *result, size_t level_count);

void mnd_args_result_set_flag(mnd_args_result_t *result, mnd_args_ref_t ref);
void mnd_args_result_set_value(mnd_args_result_t *result, mnd_args_ref_t ref, const char *value);

/* value を ',' で区切って追加する。空の項目があれば何も追加せず MND_ARGS_USAGE_ERROR、確保の失敗は MND_ARGS_OUT_OF_MEMORY */
mnd_args_status_t mnd_args_result_append_list(mnd_args_result_t *result, mnd_args_ref_t ref,
                                              const char *value, bool explicit_set);
bool mnd_args_result_add_positional(mnd_args_result_t *result, const char *arg);

/* 経路上のすべての段で、明示的に指定されていないオプションに既定値を適用する */
mnd_args_status_t mnd_args_result_apply_defaults(mnd_args_result_t *result);

#endif
