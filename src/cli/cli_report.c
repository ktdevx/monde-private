/* Reporter: 診断、使い方のエラー、統計、情報の書式（すべて英語、1 件 1 行、改行は \n） */

#include "cli/cli_report.h"

#include "clopts/help.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

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

/* text が "<prefix>: " で始まるなら、その直後を返す。でなければ NULL */
static const char *skip_prefix(const char *text, const char *prefix)
{
    size_t len = strlen(prefix);

    if ((len == 0) || (strncmp(text, prefix, len) != 0))
    {
        return NULL;
    }
    text += len;

    if ((text[0] != ':') || (text[1] != ' '))
    {
        return NULL;
    }

    return text + 2;
}

/*
 * cli-args の使い方のエラーのメッセージから接頭辞を除く。経路の文字列 path による "<path>: "、
 * 次にプログラム名 program による "<program>: " を試し、どちらも一致しなければそのまま返す（13.4）
 */
static const char *strip_usage_prefix(const char *message, const char *path, const char *program)
{
    const char *rest = NULL;

    if (path != NULL)
    {
        rest = skip_prefix(message, path);
    }
    if ((rest == NULL) && (program != NULL))
    {
        rest = skip_prefix(message, program);
    }

    return (rest != NULL) ? rest : message;
}

/* 使い方のエラーはパスと行を付けず "monde: <severity>: <message>" で書き、最後に cli-args の案内を 1 行書く（1.6） */
void mnd_cli_report_usage_error(FILE *err, const clopts_result_t *result, const mnd_diag_list_t *diags)
{
    char *path_text = clopts_format_path(result);  /* NULL（メモリ不足）なら経路の接頭辞を除かない */
    char *hint;
    const char *program = NULL;
    size_t count = mnd_diag_list_count(diags);
    size_t i;

    if (result->program != NULL)
    {
        program = result->program->name;
    }

    for (i = 0; i < count; i++)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);

        fprintf(err, "%s: %s: %s\n", REPORT_PROGRAM, severity_name(diag->severity),
                strip_usage_prefix(message_text(diag), path_text, program));
    }
    write_dropped(err, diags);

    hint = clopts_format_usage_hint(result);
    if (hint != NULL)
    {
        fwrite(hint, 1, strlen(hint), err);
    }
    else
    {
        fprintf(err, "%s: error: %s\n", REPORT_PROGRAM, "out of memory");
    }
    clopts_text_free(hint);
    clopts_text_free(path_text);
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
