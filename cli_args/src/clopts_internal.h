#ifndef CLOPTS_INTERNAL_H
#define CLOPTS_INTERNAL_H

/* モジュール内部の宣言。src/ の .c と tests/ だけが include する */

#include "clopts/clopts.h"
#include "clopts/errors.h"

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
typedef struct clopts_ref
{
    size_t level;
    size_t index;
} clopts_ref_t;

/* 宣言が妥当なら true。誤りごとにエラーを追加して false を返す */
bool clopts_decl_check(const clopts_program_t *program, clopts_errors_t *errors);

/* parent の子コマンドを名前で探す。見つからなければ NULL */
const clopts_command_t *clopts_find_child(const clopts_command_t *parent,
                                          const char *name, size_t name_len);

/* 有効なオプションを名前で探す。見つかれば *ref に位置を入れて true（12.1、12.2） */
bool clopts_find_long(const clopts_result_t *result, const char *name, size_t name_len,
                      clopts_ref_t *ref);
bool clopts_find_short(const clopts_result_t *result, char short_name, clopts_ref_t *ref);

/*
 * name が "no-" で始まり、残り（1 バイト以上）が有効なオプションの長い名前と一致し、
 * そのオプションが negatable のフラグなら、*ref に位置を入れて true（14.2、14.8、14.9）
 */
bool clopts_find_negated(const clopts_result_t *result, const char *name, size_t name_len,
                         clopts_ref_t *ref);

/* 有効なオプションの列挙。順序は「最後の段のオプション、親の引き継ぐオプション、…、ルートの引き継ぐオプション」 */
size_t clopts_effective_count(const clopts_result_t *result);
clopts_ref_t clopts_effective_at(const clopts_result_t *result, size_t i); /* i < count */

/* ref が指すオプションの宣言 */
const clopts_option_t *clopts_option_at(const clopts_result_t *result, clopts_ref_t ref);

/* option->value_name が NULL なら "VALUE" */
const char *clopts_value_name(const clopts_option_t *option);

/* min(index, positional_count - 1) 番目の名前 */
const char *clopts_positional_name(const clopts_command_t *command, size_t index);

/* ResultStore（args_result.c）。Parser だけが使う */

/* command を新しい段として加え、値の配列を確保する。失敗はメモリ不足 */
bool clopts_result_push(clopts_result_t *result, const clopts_command_t *command);

/* 段を level_count 個まで減らし、取り除いた段の値を解放する（help の語の誤りで使う） */
void clopts_result_truncate(clopts_result_t *result, size_t level_count);

/* フラグを「明示的に指定された」にし、有効か無効かを記録する（5.5、14.2、14.4） */
void clopts_result_set_flag(clopts_result_t *result, clopts_ref_t ref, bool enabled);
void clopts_result_set_value(clopts_result_t *result, clopts_ref_t ref, const char *value);

/* value を ',' で区切って追加する。空の項目があれば何も追加せず CLOPTS_USAGE_ERROR、確保の失敗は CLOPTS_OUT_OF_MEMORY */
clopts_status_t clopts_result_append_list(clopts_result_t *result, clopts_ref_t ref,
                                          const char *value, bool explicit_set);
bool clopts_result_add_positional(clopts_result_t *result, const char *arg);

/* 経路上のすべての段で、明示的に指定されていないオプションに既定値を適用する（既定で有効のフラグを含む） */
clopts_status_t clopts_result_apply_defaults(clopts_result_t *result);

#endif
