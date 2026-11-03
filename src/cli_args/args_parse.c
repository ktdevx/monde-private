#include "cli_args/args.h"
#include "cli_args/args_internal.h"

/* Parser: 引数列の走査と状態の決定 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 走査の状態。サブコマンドを選択した後のトークンを左から順に見る */
typedef struct mnd_args_scan
{
    const mnd_args_program_t *program;
    const mnd_args_command_t *command;  /* 選択されたサブコマンド */
    int argc;
    char *const *argv;
    int next;                           /* 次に見るトークンの添字 */
    mnd_args_result_t *result;
    mnd_diag_list_t *diags;
} mnd_args_scan_t;

static mnd_args_status_t finish(mnd_args_result_t *result, mnd_args_status_t status)
{
    result->status = status;

    return status;
}

static mnd_args_status_t out_of_memory(mnd_args_result_t *result, mnd_diag_list_t *diags)
{
    mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory while parsing arguments");

    return finish(result, MND_ARGS_OUT_OF_MEMORY);
}

/* サブコマンドの選択後の使い方のエラー。メッセージは "<program> <command>: " で始める */
static mnd_args_status_t scan_unknown_option(mnd_args_scan_t *scan, const char *token)
{
    mnd_diag_add(scan->diags, MND_SEVERITY_ERROR, NULL, 0, "%s %s: unknown option '%s'",
                 scan->program->name, scan->command->name, token);

    return finish(scan->result, MND_ARGS_USAGE_ERROR);
}

/*
 * サブコマンドの後のトークンを走査する。
 * "-" 単独と "-" で始まらないトークンは位置引数として記録する。
 * "-" で始まるその他のトークンは、名前によるオプションの検索を経ずに未知のオプションとする
 */
static mnd_args_status_t scan_command(mnd_args_scan_t *scan)
{
    mnd_args_status_t status;

    while (scan->next < scan->argc)
    {
        const char *token = scan->argv[scan->next];

        scan->next++;

        if ((token[0] == '-') && (token[1] != '\0'))
        {
            return scan_unknown_option(scan, token);
        }

        if (!mnd_args_result_add_positional(scan->result, token))
        {
            return out_of_memory(scan->result, scan->diags);
        }
    }

    status = mnd_args_result_apply_defaults(scan->result);
    if (status == MND_ARGS_OUT_OF_MEMORY)
    {
        return out_of_memory(scan->result, scan->diags);
    }

    return finish(scan->result, status);
}

/* サブコマンドを選択し、残りのトークンを走査する */
static mnd_args_status_t select_command(const mnd_args_program_t *program,
                                        const mnd_args_command_t *command, int argc,
                                        char *const *argv, mnd_args_result_t *result,
                                        mnd_diag_list_t *diags)
{
    mnd_args_scan_t scan;

    if (!mnd_args_result_begin(result, command))
    {
        return out_of_memory(result, diags);
    }

    scan.program = program;
    scan.command = command;
    scan.argc = argc;
    scan.argv = argv;
    scan.next = 2;
    scan.result = result;
    scan.diags = diags;

    return scan_command(&scan);
}

/* "help" の後のトークンを振り分ける（8.1、8.3、8.4） */
static mnd_args_status_t dispatch_help(const mnd_args_program_t *program, int argc,
                                       char *const *argv, mnd_args_result_t *result,
                                       mnd_diag_list_t *diags)
{
    const mnd_args_command_t *command;
    const char *name;

    if (argc <= 2)
    {
        return finish(result, MND_ARGS_HELP);
    }

    if (argc > 3)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "%s: too many arguments for 'help'",
                     program->name);

        return finish(result, MND_ARGS_USAGE_ERROR);
    }

    name = argv[2];
    command = mnd_args_find_command(program, name, strlen(name));
    if (command == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "%s: unknown command '%s'",
                     program->name, name);

        return finish(result, MND_ARGS_USAGE_ERROR);
    }

    result->command = command;

    return finish(result, MND_ARGS_HELP);
}

mnd_args_status_t mnd_args_parse(const mnd_args_program_t *program, int argc, char *const *argv,
                                 mnd_args_result_t *result, mnd_diag_list_t *diags)
{
    const mnd_args_command_t *command;
    const char *first;

    mnd_args_result_free(result);

    if (!mnd_args_decl_check(program, diags))
    {
        return finish(result, MND_ARGS_DECL_ERROR);
    }

    if (argc <= 1)
    {
        return finish(result, MND_ARGS_NO_COMMAND);
    }

    first = argv[1];
    if ((strcmp(first, "--help") == 0) || (strcmp(first, "-h") == 0))
    {
        return finish(result, MND_ARGS_HELP);
    }

    if (strcmp(first, "--version") == 0)
    {
        return finish(result, MND_ARGS_VERSION);
    }

    if (strcmp(first, "help") == 0)
    {
        return dispatch_help(program, argc, argv, result, diags);
    }

    command = mnd_args_find_command(program, first, strlen(first));
    if (command != NULL)
    {
        return select_command(program, command, argc, argv, result, diags);
    }

    if ((first[0] == '-') && (first[1] != '\0'))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "%s: unknown option '%s'",
                     program->name, first);
    }
    else
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "%s: unknown command '%s'",
                     program->name, first);
    }

    return finish(result, MND_ARGS_USAGE_ERROR);
}
