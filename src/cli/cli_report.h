#ifndef MONDE_CLI_CLI_REPORT_H
#define MONDE_CLI_CLI_REPORT_H

/*
 * Reporter: 診断、使い方のエラー、統計、情報の文面を err に書く（すべて英語、1 件 1 行）。
 * モジュール内部のヘッダ（src/cli/ と tests/cli/ だけが include する）
 */

#include "cli_args/args.h"
#include "foundation/diag.h"
#include "po_merge/merge.h"
#include "tu/tu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* 診断を順にすべて書く。dropped なら最後に "monde: error: some diagnostics were dropped (out of memory)" */
void mnd_cli_report_diags(FILE *err, const mnd_diag_list_t *diags);

/*
 * 使い方のエラー。diags の診断を "monde: error: <message>" の形で書き、
 * 続けて mnd_args_format_usage_hint の案内（command が NULL なら全体の案内）を書く
 */
void mnd_cli_report_usage_error(FILE *err, const mnd_args_program_t *program,
                                const mnd_args_command_t *command, const mnd_diag_list_t *diags);

/* 統計と情報。quiet なら何も書かない */
void mnd_cli_report_extract(FILE *err, bool quiet, size_t messages);
void mnd_cli_report_init(FILE *err, bool quiet, size_t messages, const char *language);
void mnd_cli_report_init_language_source(FILE *err, bool quiet, const char *language, const char *variable);
void mnd_cli_report_update(FILE *err, bool quiet, const mnd_merge_stats_t *stats);
void mnd_cli_report_translate(FILE *err, bool quiet, const mnd_tu_stats_t *stats);

#endif
