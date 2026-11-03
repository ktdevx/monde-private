#include "cli_args/args_argv.h"

#include <stddef.h>
#include <stdlib.h>

#ifdef _WIN32
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include "foundation/wide.h"
#endif

/* ArgvSource: 引数列の UTF-8 での取得（Windows では wide で UTF-8 に変換する） */

static void mnd_args_argv_reset(mnd_args_argv_t *args)
{
    args->argc = 0;
    args->argv = NULL;
    args->owned = false;
}

void mnd_args_argv_free(mnd_args_argv_t *args)
{
    int i;

    if (args->owned && (args->argv != NULL))
    {
        for (i = 0; i < args->argc; i++)
        {
            free(args->argv[i]);
        }
        free(args->argv);
    }
    mnd_args_argv_reset(args);
}

#ifndef _WIN32

bool mnd_args_argv_get(int argc, char **argv, mnd_args_argv_t *out, mnd_diag_list_t *diags)
{
    (void)diags;

    /* Windows 以外では、渡された引数列をコピーも変換もせずに返す */
    out->argc = argc;
    out->argv = argv;
    out->owned = false;

    return true;
}

#else

bool mnd_args_argv_from_wide(int argc, wchar_t *const *wargv, mnd_args_argv_t *out,
                             mnd_diag_list_t *diags)
{
    char **argv;
    const char *reason;
    size_t count;
    int i;

    mnd_args_argv_reset(out);
    if (argc < 0)
    {
        argc = 0;
    }

    /* 末尾の NULL を含めた要素数を、確保の前に桁あふれしないか検査する */
    count = (size_t)argc;
    if (count > ((SIZE_MAX / sizeof(char *)) - 1))
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory while parsing arguments");

        return false;
    }
    /* calloc で確保し、未変換の要素を NULL にしておく（失敗時の解放を一様にする） */
    argv = (char **)calloc(count + 1, sizeof(char *));
    if (argv == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory while parsing arguments");

        return false;
    }

    for (i = 0; i < argc; i++)
    {
        reason = NULL;
        argv[i] = mnd_wide_to_utf8(wargv[i], &reason);
        if (argv[i] == NULL)
        {
            if ((reason != NULL) && (strcmp(reason, "out of memory") == 0))
            {
                mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                             "out of memory while parsing arguments");
            }
            else
            {
                mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0,
                             "command-line argument %d is not valid UTF-16", i);
            }
            out->argc = i;
            out->argv = argv;
            out->owned = true;
            mnd_args_argv_free(out);

            return false;
        }
    }

    out->argc = argc;
    out->argv = argv;
    out->owned = true;

    return true;
}

bool mnd_args_argv_get(int argc, char **argv, mnd_args_argv_t *out, mnd_diag_list_t *diags)
{
    LPWSTR *wargv;
    int wargc = 0;
    bool ok;

    /* Windows では main の引数（ANSI コードページ）を使わず、UTF-16 のコマンドラインから取得する */
    (void)argc;
    (void)argv;

    mnd_args_argv_reset(out);
    wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "cannot read the command line");

        return false;
    }

    ok = mnd_args_argv_from_wide(wargc, (wchar_t *const *)wargv, out, diags);
    LocalFree(wargv);

    return ok;
}

#endif
