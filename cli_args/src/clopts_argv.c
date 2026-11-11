#include "clopts/argv.h"
#include "clopts_internal.h"

#include <stddef.h>
#include <stdlib.h>

#ifdef _WIN32
#include <limits.h>
#include <stdint.h>
#include <wchar.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

/* ArgvSource: 引数列の UTF-8 での取得（Windows では UTF-16 のコマンドラインを UTF-8 に変換する） */

static void clopts_argv_reset(clopts_argv_t *args)
{
    args->argc = 0;
    args->argv = NULL;
    args->owned = false;
}

void clopts_argv_free(clopts_argv_t *args)
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

    clopts_argv_reset(args);
}

#ifndef _WIN32

bool clopts_argv_get(int argc, char **argv, clopts_argv_t *out, clopts_errors_t *errors)
{
    (void)errors;

    /* Windows 以外では、渡された引数列をコピーも変換もせずに返す */
    out->argc = argc;
    out->argv = argv;
    out->owned = false;

    return true;
}

#else

/* UTF-16 から UTF-8 への変換の失敗の種類 */
typedef enum wide_failure
{
    WIDE_OUT_OF_MEMORY,  /* 確保できなかった（API に渡せない長さを含む） */
    WIDE_INVALID         /* 不正な UTF-16（対になっていないサロゲートなど） */
} wide_failure_t;

/*
 * NUL 終端の UTF-16 を、確保した NUL 終端の UTF-8 に変換する。呼び出し側が free で解放する。
 * 失敗時は NULL を返し、*failure に失敗の種類を入れる
 */
static char *wide_to_utf8(const wchar_t *wide, wide_failure_t *failure)
{
    size_t len;
    char *utf8;
    int n;

    /* NUL 終端を含めた長さを int で API に渡すため、範囲を超える入力は確保できないものとして扱う */
    len = wcslen(wide);
    if (len >= (size_t)INT_MAX)
    {
        *failure = WIDE_OUT_OF_MEMORY;
        return NULL;
    }

    n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, (int)len + 1, NULL, 0, NULL, NULL);
    if (n <= 0)
    {
        *failure = WIDE_INVALID;
        return NULL;
    }

    utf8 = (char *)malloc((size_t)n);
    if (utf8 == NULL)
    {
        *failure = WIDE_OUT_OF_MEMORY;
        return NULL;
    }

    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, (int)len + 1, utf8, n, NULL, NULL) != n)
    {
        free(utf8);
        *failure = WIDE_INVALID;
        return NULL;
    }

    return utf8;
}

bool clopts_argv_from_wide(int argc, wchar_t *const *wargv, clopts_argv_t *out,
                           clopts_errors_t *errors)
{
    char **argv;
    wide_failure_t failure = WIDE_INVALID;
    size_t count;
    int i;

    clopts_argv_reset(out);
    if (argc < 0)
    {
        argc = 0;
    }

    /* 末尾の NULL を含めた要素数を、確保の前に桁あふれしないか検査する */
    count = (size_t)argc;
    if (count > ((SIZE_MAX / sizeof(char *)) - 1))
    {
        clopts_errors_add(errors, "out of memory while parsing arguments");
        return false;
    }

    /* calloc で確保し、未変換の要素を NULL にしておく（失敗時の解放を一様にする） */
    argv = (char **)calloc(count + 1, sizeof(char *));
    if (argv == NULL)
    {
        clopts_errors_add(errors, "out of memory while parsing arguments");
        return false;
    }

    for (i = 0; i < argc; i++)
    {
        argv[i] = wide_to_utf8(wargv[i], &failure);
        if (argv[i] == NULL)
        {
            if (failure == WIDE_OUT_OF_MEMORY)
            {
                clopts_errors_add(errors, "out of memory while parsing arguments");
            }
            else
            {
                clopts_errors_add(errors, "command-line argument %d is not valid UTF-16", i);
            }

            out->argc = i;
            out->argv = argv;
            out->owned = true;
            clopts_argv_free(out);

            return false;
        }
    }

    out->argc = argc;
    out->argv = argv;
    out->owned = true;

    return true;
}

bool clopts_argv_get(int argc, char **argv, clopts_argv_t *out, clopts_errors_t *errors)
{
    LPWSTR *wargv;
    int wargc = 0;
    bool ok;

    /* Windows では main の引数（ANSI コードページ）を使わず、UTF-16 のコマンドラインから取得する */
    (void)argc;
    (void)argv;

    clopts_argv_reset(out);
    wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv == NULL)
    {
        clopts_errors_add(errors, "cannot read the command line");
        return false;
    }

    ok = clopts_argv_from_wide(wargc, (wchar_t *const *)wargv, out, errors);
    LocalFree(wargv);

    return ok;
}

#endif
