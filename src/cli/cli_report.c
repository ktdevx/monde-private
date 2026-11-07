/* Reporter: 診断、使い方のエラー、統計、情報の書式（すべて英語、1 件 1 行、改行は \n） */

#include "cli/cli_report.h"

#include "cli_args/args_help.h"
#include "foundation/buf.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* 診断に付けるプログラム名（パスがないとき） */
#define REPORT_PROGRAM "monde"

/* dropped のときに最後に足す固定のメッセージ */
#define REPORT_DROPPED_MESSAGE "some diagnostics were dropped (out of memory)"

static const char *severity_name(mnd_severity_t severity)
{
    return (severity == MND_SEVERITY_WARNING) ? "warning" : "error";
}

static const char *message_text(const mnd_diag_t *diag)
{
    return (diag->message != NULL) ? diag->message : "";
}

static void write_dropped(FILE *err, const mnd_diag_list_t *diags)
{
    if (diags->dropped)
    {
        fprintf(err, "%s: error: %s\n", REPORT_PROGRAM, REPORT_DROPPED_MESSAGE);
    }
}

/*
 * パスと行があれば "<path>:<line>: <severity>: <message>"、パスだけなら "<path>: <severity>: <message>"、
 * パスがなければ（行の有無にかかわらず）"monde: <severity>: <message>"（9.2、9.3）
 */
void mnd_cli_report_diags(FILE *err, const mnd_diag_list_t *diags)
{
    size_t count = mnd_diag_list_count(diags);
    size_t i;

    for (i = 0; i < count; i++)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);

        if ((diag->path != NULL) && (diag->line > 0))
        {
            fprintf(err, "%s:%zu: %s: %s\n", diag->path, diag->line, severity_name(diag->severity),
                    message_text(diag));
        }
        else if (diag->path != NULL)
        {
            fprintf(err, "%s: %s: %s\n", diag->path, severity_name(diag->severity), message_text(diag));
        }
        else
        {
            fprintf(err, "%s: %s: %s\n", REPORT_PROGRAM, severity_name(diag->severity), message_text(diag));
        }
    }
    write_dropped(err, diags);
}

/* 使い方のエラーはパスと行を付けず "monde: <severity>: <message>" で書き、最後に cli-args の案内を 1 行書く（1.6） */
void mnd_cli_report_usage_error(FILE *err, const mnd_args_program_t *program,
                                const mnd_args_command_t *command, const mnd_diag_list_t *diags)
{
    mnd_buf_t hint = MND_BUF_INIT;
    size_t count = mnd_diag_list_count(diags);
    size_t i;

    for (i = 0; i < count; i++)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);

        fprintf(err, "%s: %s: %s\n", REPORT_PROGRAM, severity_name(diag->severity), message_text(diag));
    }
    write_dropped(err, diags);

    if (mnd_args_format_usage_hint(program, command, &hint))
    {
        fwrite(mnd_buf_cstr(&hint), 1, hint.len, err);
    }
    else
    {
        fprintf(err, "%s: error: %s\n", REPORT_PROGRAM, "out of memory");
    }
    mnd_buf_free(&hint);
}

void mnd_cli_report_extract(FILE *err, bool quiet, size_t messages)
{
    if (!quiet)
    {
        fprintf(err, "extract: %zu messages\n", messages);
    }
}

void mnd_cli_report_init(FILE *err, bool quiet, size_t messages, const char *language)
{
    if (!quiet)
    {
        fprintf(err, "init: %zu messages, language %s\n", messages, language);
    }
}

void mnd_cli_report_init_language_source(FILE *err, bool quiet, const char *language, const char *variable)
{
    if (!quiet)
    {
        fprintf(err, "init: using language '%s' from %s\n", language, variable);
    }
}

void mnd_cli_report_update(FILE *err, bool quiet, const mnd_merge_stats_t *stats)
{
    if (!quiet)
    {
        fprintf(err, "update: %zu matched, %zu fuzzy, %zu added, %zu obsoleted\n",
                stats->matched, stats->fuzzy, stats->added, stats->obsoleted);
    }
}

/*
 * 翻訳率は translated ÷ total を 0.1% 単位に四捨五入する（整数演算）。fuzzy は翻訳済みに数えない（8.5）。
 * 合計が 0 なら翻訳率を出さない（8.6）
 */
void mnd_cli_report_translate(FILE *err, bool quiet, const mnd_tu_stats_t *stats)
{
    size_t permille;

    if (quiet)
    {
        return;
    }
    if (stats->total == 0)
    {
        fputs("translate: no translatable messages\n", err);
        return;
    }

    permille = ((stats->translated * 1000) + (stats->total / 2)) / stats->total;
    fprintf(err,
            "translate: %zu translated, %zu fuzzy, %zu untranslated, %zu missing, %zu total, "
            "%zu.%zu%% translated\n",
            stats->translated, stats->fuzzy, stats->untranslated, stats->missing, stats->total,
            permille / 10, permille % 10);
}
