#ifndef MONDE_CLI_CLI_CLOPTS_H
#define MONDE_CLI_CLI_CLOPTS_H

/*
 * Bridge: 引数パーサ CLOpts のエラーの列を、Monde の診断の列に変換する。
 * 解析の直後（src/cli/cli_run.c）と、引数列の取得の失敗時（src/monde/main.c）に使う
 */

#include "clopts/errors.h"
#include "foundation/diag.h"

/*
 * errors の各メッセージを、エラーの診断（パスなし、行番号 0）として diags の末尾に同じ順序で追加する。
 * メッセージ中の % は書式として解釈しない。errors が dropped なら diags も dropped にする。
 * errors と diags は NULL でなく、初期化済みであること
 */
void mnd_cli_clopts_errors_to_diags(const clopts_errors_t *errors, mnd_diag_list_t *diags);

#endif
