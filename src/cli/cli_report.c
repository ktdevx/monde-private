/* Reporter: 診断、使い方のエラー、統計、情報の書式 */

#include "cli/cli_report.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* 仮の実装: 何も書かない（以下の関数も同じ。書式は 2.3 で作る） */
void mnd_cli_report_diags(FILE *err, const mnd_diag_list_t *diags)
{
    (void)err;
    (void)diags;
}

void mnd_cli_report_usage_error(FILE *err, const mnd_args_program_t *program,
                                const mnd_args_command_t *command, const mnd_diag_list_t *diags)
{
    (void)err;
    (void)program;
    (void)command;
    (void)diags;
}

void mnd_cli_report_extract(FILE *err, bool quiet, size_t messages)
{
    (void)err;
    (void)quiet;
    (void)messages;
}

void mnd_cli_report_init(FILE *err, bool quiet, size_t messages, const char *language)
{
    (void)err;
    (void)quiet;
    (void)messages;
    (void)language;
}

void mnd_cli_report_init_language_source(FILE *err, bool quiet, const char *language, const char *variable)
{
    (void)err;
    (void)quiet;
    (void)language;
    (void)variable;
}

void mnd_cli_report_update(FILE *err, bool quiet, const mnd_merge_stats_t *stats)
{
    (void)err;
    (void)quiet;
    (void)stats;
}

void mnd_cli_report_translate(FILE *err, bool quiet, const mnd_tu_stats_t *stats)
{
    (void)err;
    (void)quiet;
    (void)stats;
}
