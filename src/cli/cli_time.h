#ifndef MONDE_CLI_CLI_TIME_H
#define MONDE_CLI_CLI_TIME_H

/*
 * Clock: env->now の時刻を、ローカル時刻と UTC オフセットで PO の日時文字列にする。
 * モジュール内部のヘッダ（src/cli/ と tests/cli/ だけが include する）
 */

#include "cli/cli.h"
#include "foundation/diag.h"
#include "po/po.h"

#include <stdbool.h>

/* "YYYY-MM-DD HH:MM+ZZZZ" を out に書く。失敗（localtime / gmtime が NULL、整形の失敗）なら診断を追加して false */
bool mnd_cli_format_now(const mnd_cli_env_t *env, char out[MND_PO_DATE_SIZE], mnd_diag_list_t *diags);

#endif
