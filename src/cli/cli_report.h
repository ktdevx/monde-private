#ifndef MONDE_CLI_CLI_REPORT_H
#define MONDE_CLI_CLI_REPORT_H

/*
 * Reporter: 診断、使い方のエラー、統計、情報の文面を err に書く（すべて英語、1 件 1 行）。
 * モジュール内部のヘッダ（src/cli/ と tests/cli/ だけが include する）
 */

#include "clopts/clopts.h"
#include "foundation/diag.h"
#include "po_merge/merge.h"
#include "tu/tu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/*
 * 診断を順にすべて書く。"<path>:<line>: <severity>: <message>"、行がなければ "<path>: <severity>: <message>"、
 * パスがなければ "monde: <severity>: <message>"（<severity> は error / warning）。
 * dropped なら最後に "monde: error: some diagnostics were dropped (out of memory)"
 */
void mnd_cli_report_diags(FILE *err, const mnd_diag_list_t *diags);

/*
 * 使い方のエラー。diags の診断を、result の経路の文字列 "<path>: " の接頭辞を除いて
 * "monde: error: <message>" の形で書き、続けて clopts_format_usage_hint の案内（経路の最後のコマンドの案内）を書く。
 * result は clopts_parse が CLOPTS_USAGE_ERROR を返したものであること
 */
void mnd_cli_report_usage_error(FILE *err, const clopts_result_t *result, const mnd_diag_list_t *diags);

/* 統計と情報。quiet なら何も書かない */
void mnd_cli_report_extract(FILE *err, bool quiet, size_t messages);
void mnd_cli_report_init(FILE *err, bool quiet, size_t messages, const char *language);
void mnd_cli_report_init_language_source(FILE *err, bool quiet, const char *language, const char *variable);
void mnd_cli_report_update(FILE *err, bool quiet, const mnd_merge_stats_t *stats);
void mnd_cli_report_translate(FILE *err, bool quiet, const mnd_tu_stats_t *stats);

#endif
