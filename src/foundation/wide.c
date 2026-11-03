#include "foundation/wide.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

wchar_t *mnd_wide_from_utf8(const char *utf8, const char **reason)
{
    size_t len;
    wchar_t *wide;
    int n;

    /* NUL 終端を含めた長さを int で API に渡すため、範囲を超える入力は確保できないものとして扱う */
    len = strlen(utf8);
    if (len >= (size_t)INT_MAX)
    {
        *reason = "out of memory";
        return NULL;
    }

    n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, (int)len + 1, NULL, 0);
    if (n <= 0)
    {
        *reason = "invalid UTF-8";
        return NULL;
    }

    wide = malloc((size_t)n * sizeof *wide);
    if (wide == NULL)
    {
        *reason = "out of memory";
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, (int)len + 1, wide, n) != n)
    {
        free(wide);
        *reason = "invalid UTF-8";
        return NULL;
    }

    return wide;
}

char *mnd_wide_to_utf8(const wchar_t *wide, const char **reason)
{
    size_t len;
    char *utf8;
    int n;

    /* NUL 終端を含めた長さを int で API に渡すため、範囲を超える入力は確保できないものとして扱う */
    len = wcslen(wide);
    if (len >= (size_t)INT_MAX)
    {
        *reason = "out of memory";
        return NULL;
    }

    n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, (int)len + 1, NULL, 0, NULL, NULL);
    if (n <= 0)
    {
        *reason = "invalid UTF-16";
        return NULL;
    }

    utf8 = malloc((size_t)n);
    if (utf8 == NULL)
    {
        *reason = "out of memory";
        return NULL;
    }

    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, (int)len + 1, utf8, n, NULL, NULL) != n)
    {
        free(utf8);
        *reason = "invalid UTF-16";
        return NULL;
    }

    return utf8;
}
