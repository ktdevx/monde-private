/* Language: --language と環境変数からの言語コードの決定と検査 */

#include "cli/cli_language.h"

#include "foundation/buf.h"
#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 言語コードを引く環境変数（この順に見る） */
static const char *const language_variables[] = { "LC_ALL", "LC_MESSAGES", "LANG" };

/* env->getenv が NULL のときに使う標準の getenv */
static const char *language_standard_getenv(const char *name)
{
    return getenv(name);
}

/* "." / "@" 以降を除いた長さ len の値が未設定（空・"C"・"POSIX"）とみなせるなら true */
static bool language_is_unset(const char *value, size_t len)
{
    if (len == 0)
    {
        return true;
    }
    if ((len == 1) && (value[0] == 'C'))
    {
        return true;
    }
    return ((len == 5) && (memcmp(value, "POSIX", 5) == 0));
}

/*
 * 長さ len の値が言語コードとして使えるなら true。空、空白・制御文字、
 * 非 ASCII のバイト（Windows の getenv は ANSI の場合があるため ASCII に限る）を含めば false
 */
static bool language_is_valid(const char *value, size_t len)
{
    size_t i;

    if (len == 0)
    {
        return false;
    }
    for (i = 0; i < len; i++)
    {
        unsigned char byte = (unsigned char)value[i];

        if ((byte <= 0x20) || (byte >= 0x7F))
        {
            return false;
        }
    }
    return true;
}

/*
 * 不正な言語コードの診断を加える。値の空白と表示できる ASCII はそのまま、
 * 制御文字と非 ASCII のバイトは \xHH の形で示す。source は "--language" か環境変数名
 */
static void language_report_invalid(const char *value, size_t len, const char *source,
                                    mnd_diag_list_t *diags)
{
    mnd_buf_t shown = MND_BUF_INIT;
    bool ok = true;
    size_t i;

    for (i = 0; ok && (i < len); i++)
    {
        unsigned char byte = (unsigned char)value[i];

        if ((byte >= 0x20) && (byte < 0x7F))
        {
            ok = mnd_buf_append_byte(&shown, byte);
        }
        else
        {
            char escaped[5];

            (void)snprintf(escaped, sizeof(escaped), "\\x%02X", (unsigned int)byte);
            ok = mnd_buf_append_str(&shown, escaped);
        }
    }

    if (ok)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "invalid language code '%s' from %s",
                     mnd_buf_cstr(&shown), source);
    }
    else
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "invalid language code from %s", source);
    }
    mnd_buf_free(&shown);
}

/* 長さ len の値を検査し、通れば複製して out に入れる。失敗なら診断を加えて false */
static bool language_accept(const char *value, size_t len, const char *variable,
                            mnd_cli_language_t *out, mnd_diag_list_t *diags)
{
    char *code;

    if (!language_is_valid(value, len))
    {
        language_report_invalid(value, len, (variable != NULL) ? variable : "--language", diags);
        return false;
    }

    code = (char *)malloc(len + 1);
    if (code == NULL)
    {
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "out of memory");
        return false;
    }
    memcpy(code, value, len);
    code[len] = '\0';

    out->code = code;
    out->variable = variable;
    return true;
}

bool mnd_cli_resolve_language(const mnd_cli_env_t *env, const char *explicit,
                              mnd_cli_language_t *out, mnd_diag_list_t *diags)
{
    mnd_cli_getenv_fn lookup = (env->getenv != NULL) ? env->getenv : language_standard_getenv;
    size_t i;

    out->code = NULL;
    out->variable = NULL;

    /* --language の値は "." / "@" を除かずにそのまま使う（5.2） */
    if (explicit != NULL)
    {
        return language_accept(explicit, strlen(explicit), NULL, out, diags);
    }

    for (i = 0; i < (sizeof(language_variables) / sizeof(language_variables[0])); i++)
    {
        const char *value = lookup(language_variables[i]);
        size_t len;

        if (value == NULL)
        {
            continue;
        }
        len = strcspn(value, ".@");
        if (language_is_unset(value, len))
        {
            continue;
        }
        /* 最初に決まった値が不正なら、次の環境変数は見ずにエラーにする（5.7） */
        return language_accept(value, len, language_variables[i], out, diags);
    }

    mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "cannot determine the language; specify --language");
    return false;
}
