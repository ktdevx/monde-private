/* Run: 引数の解析結果の分岐、ヘルプ・バージョン・使い方の表示、ハンドラの呼び出し、終了コード */

#include "cli/cli.h"
#include "cli/cli_clopts.h"
#include "cli/cli_commands.h"
#include "cli/cli_internal.h"
#include "cli/cli_report.h"

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "clopts/help.h"
#include "foundation/diag.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* 文面の生成に失敗したとき（メモリ不足）の診断 */
#define RUN_OUT_OF_MEMORY_MESSAGE "out of memory"

/* NUL 終端の text を fp に書いて flush する。書き込みに失敗したら false */
static bool write_cstr(FILE *fp, const char *text)
{
    size_t len = strlen(text);
    bool ok = true;

    if ((len > 0) && (fwrite(text, 1, len, fp) != len))
    {
        ok = false;
    }
    if (fflush(fp) != 0)
    {
        ok = false;
    }

    return ok;
}

/* 診断を 1 件だけ作って err に書く（ハンドラの外で起きた内部エラー用） */
static void report_single_error(FILE *err, const char *message)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    mnd_diag_add(&diags, MND_SEVERITY_ERROR, NULL, 0, "%s", message);
    mnd_cli_report_diags(err, &diags);
    mnd_diag_list_free(&diags);
}

/*
 * ヘルプ（result の経路の最後のコマンド）またはバージョンの文面を fp に書き、成功なら exit_code を返す。
 * 文面の生成や書き込みに失敗したら err に診断を書いて 1 を返す
 */
static int write_text(const mnd_cli_env_t *env, FILE *fp, const mnd_args_result_t *result, bool version,
                      int exit_code)
{
    char *text;
    int code = exit_code;

    if (version)
    {
        text = mnd_args_format_version(result->program);
    }
    else
    {
        text = mnd_args_format_help(result);
    }

    if (text == NULL)
    {
        report_single_error(env->err, RUN_OUT_OF_MEMORY_MESSAGE);
        code = MND_CLI_EXIT_FAILURE;
    }
    else if (!write_cstr(fp, text))
    {
        report_single_error(env->err, (fp == env->out) ? "failed to write to standard output"
                                                       : "failed to write to standard error");
        code = MND_CLI_EXIT_FAILURE;
    }

    mnd_args_text_free(text);

    return code;
}

/* 解析に成功したサブコマンドのハンドラを呼び、診断を表示して終了コードを返す */
static int run_handler(const mnd_cli_env_t *env, const mnd_args_result_t *result, mnd_diag_list_t *diags,
                       mnd_cli_find_handler_fn find_handler)
{
    const char *name = mnd_args_command(result)->name;
    mnd_cli_handler_fn handler = find_handler(name);
    mnd_cli_ctx_t ctx;
    bool ok;

    if (handler == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "internal error: no handler for command '%s'", name);
        mnd_cli_report_diags(env->err, diags);
        return MND_CLI_EXIT_FAILURE;
    }

    ctx.env = env;
    ctx.result = result;
    ctx.diags = diags;
    ctx.quiet = mnd_args_flag(result, MND_CLI_OPT_QUIET);
    ctx.usage_error = false;

    ok = handler(&ctx);
    mnd_cli_report_diags(env->err, diags);

    /* dropped を含め、エラーの診断があれば成功として扱わない */
    if (ok && !mnd_diag_list_has_error(diags))
    {
        return MND_CLI_EXIT_OK;
    }

    return ctx.usage_error ? MND_CLI_EXIT_USAGE : MND_CLI_EXIT_FAILURE;
}

int mnd_cli_run_with(const mnd_cli_env_t *env, int argc, char *const *argv, mnd_cli_find_handler_fn find_handler)
{
    mnd_args_program_t program = mnd_cli_program(env->version);
    mnd_args_result_t result = MND_ARGS_RESULT_INIT;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_args_status_t status;
    int exit_code;

    /* 解析のエラーは直後に診断へ移し、以降は現在と同じ diags で報告と終了コードを決める */
    status = mnd_args_parse(&program, argc, argv, &result, &errors);
    mnd_cli_clopts_errors_to_diags(&errors, &diags);
    clopts_errors_free(&errors);

    switch (status)
    {
    case MND_ARGS_OK:
        exit_code = run_handler(env, &result, &diags, find_handler);
        break;
    case MND_ARGS_HELP:
        exit_code = write_text(env, env->out, &result, false, MND_CLI_EXIT_OK);
        break;
    case MND_ARGS_VERSION:
        exit_code = write_text(env, env->out, &result, true, MND_CLI_EXIT_OK);
        break;
    case MND_ARGS_NO_COMMAND:
        exit_code = write_text(env, env->err, &result, false, MND_CLI_EXIT_USAGE);
        break;
    case MND_ARGS_USAGE_ERROR:
        mnd_cli_report_usage_error(env->err, &result, &diags);
        exit_code = MND_CLI_EXIT_USAGE;
        break;
    case MND_ARGS_DECL_ERROR:
    case MND_ARGS_OUT_OF_MEMORY:
    default:
        mnd_cli_report_diags(env->err, &diags);
        exit_code = MND_CLI_EXIT_FAILURE;
        break;
    }

    mnd_args_result_free(&result);
    mnd_diag_list_free(&diags);

    return exit_code;
}

int mnd_cli_run(const mnd_cli_env_t *env, int argc, char *const *argv)
{
    return mnd_cli_run_with(env, argc, argv, mnd_cli_find_handler);
}
