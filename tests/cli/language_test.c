/* スイート language: 優先順位、"." / "@" の除去、C / POSIX、空、不正な値（5.2〜5.7） */

/* POSIX の setenv / unsetenv を C99 の厳密なモードで宣言させる（Windows は _putenv を使う） */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif

#include <naos/naos.h>

#include "cli/cli_language.h"
#include "foundation/diag.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* 表 vars の count 行を test_env_getenv に設定し、mnd_cli_resolve_language を呼ぶ */
static bool language_resolve(const test_env_var_t *vars, size_t count, const char *explicit,
                             mnd_cli_language_t *out, mnd_diag_list_t *diags)
{
    mnd_cli_env_t env;

    memset(&env, 0, sizeof(env));
    env.getenv = test_env_getenv;
    test_env_set(vars, count);
    return mnd_cli_resolve_language(&env, explicit, out, diags);
}

/*
 * 出どころが期待どおり（どちらも NULL、または同じ名前）なら true。
 * 定数の NULL との比較をマクロに書くと MSVC の C4127 になるため、関数で比べる
 */
static bool language_same_variable(const char *expected, const char *actual)
{
    if ((expected == NULL) || (actual == NULL))
    {
        return (expected == actual);
    }
    return (strcmp(expected, actual) == 0);
}

/* 成功し、言語コードが code、出どころが variable（NULL なら明示）、診断がないことを確かめる */
#define LANGUAGE_CHECK_OK(ok, language, diags, expected_code, expected_variable)  \
    do                                                                            \
    {                                                                             \
        NAOS_CHECK_TRUE(ok);                                                      \
        NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&(diags)));                     \
        NAOS_REQUIRE_NOT_NULL((language).code);                                   \
        NAOS_CHECK_STR_EQ((expected_code), (language).code);                      \
        NAOS_CHECK_TRUE(language_same_variable((expected_variable), (language).variable)); \
    } while (0)

/* 失敗し、言語コードが NULL で、診断がエラー 1 件（パスと行なし）で message であることを確かめる */
#define LANGUAGE_CHECK_ERROR(ok, language, diags, expected_message)               \
    do                                                                            \
    {                                                                             \
        const mnd_diag_t *language_diag_;                                         \
        NAOS_CHECK_FALSE(ok);                                                     \
        NAOS_CHECK_NULL((language).code);                                         \
        NAOS_CHECK_NULL((language).variable);                                     \
        NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&(diags)));                   \
        language_diag_ = mnd_diag_list_at(&(diags), 0);                           \
        NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, language_diag_->severity);          \
        NAOS_CHECK_NULL(language_diag_->path);                                    \
        NAOS_CHECK_UINT_EQ(0, language_diag_->line);                              \
        NAOS_CHECK_STR_EQ((expected_message), language_diag_->message);           \
    } while (0)

/* --language の値は環境変数より優先し、出どころは NULL（5.2） */
NAOS_TEST(language_explicit_has_priority)
{
    static const test_env_var_t vars[] = {
        { "LC_ALL", "fr_FR.UTF-8" },
        { "LC_MESSAGES", "de_DE" },
        { "LANG", "es_ES" },
    };
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(vars, 3, "ja", &language, &diags);

    LANGUAGE_CHECK_OK(ok, language, diags, "ja", NULL);

    free((void *)language.code);
    mnd_diag_list_free(&diags);
}

/* --language の値は "." / "@" を含んでもそのまま使う（5.2） */
NAOS_TEST(language_explicit_kept_as_is)
{
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(NULL, 0, "sr_RS.UTF-8@latin", &language, &diags);

    LANGUAGE_CHECK_OK(ok, language, diags, "sr_RS.UTF-8@latin", NULL);

    free((void *)language.code);
    mnd_diag_list_free(&diags);
}

/* 3 つとも設定されていれば LC_ALL を使う（5.3） */
NAOS_TEST(language_lc_all_first)
{
    static const test_env_var_t vars[] = {
        { "LANG", "es_ES" },
        { "LC_MESSAGES", "de_DE" },
        { "LC_ALL", "fr_FR" },
    };
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(vars, 3, NULL, &language, &diags);

    LANGUAGE_CHECK_OK(ok, language, diags, "fr_FR", "LC_ALL");

    free((void *)language.code);
    mnd_diag_list_free(&diags);
}

/* LC_ALL がなければ LC_MESSAGES を LANG より先に使う（5.3） */
NAOS_TEST(language_lc_messages_before_lang)
{
    static const test_env_var_t vars[] = {
        { "LANG", "es_ES" },
        { "LC_MESSAGES", "de_DE" },
    };
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(vars, 2, NULL, &language, &diags);

    LANGUAGE_CHECK_OK(ok, language, diags, "de_DE", "LC_MESSAGES");

    free((void *)language.code);
    mnd_diag_list_free(&diags);
}

/* LANG だけが設定されていれば LANG を使う（5.3） */
NAOS_TEST(language_lang_last)
{
    static const test_env_var_t vars[] = {
        { "LANG", "es_ES" },
    };
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(vars, 1, NULL, &language, &diags);

    LANGUAGE_CHECK_OK(ok, language, diags, "es_ES", "LANG");

    free((void *)language.code);
    mnd_diag_list_free(&diags);
}

/* 環境変数の値から "." 以降と "@" 以降を除く（5.3） */
NAOS_TEST(language_strips_codeset_and_modifier)
{
    static const test_env_var_t codeset_and_modifier[] = {
        { "LANG", "ja_JP.UTF-8@euro" },
    };
    static const test_env_var_t modifier_only[] = {
        { "LC_MESSAGES", "sr_RS@latin" },
    };
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(codeset_and_modifier, 1, NULL, &language, &diags);

    LANGUAGE_CHECK_OK(ok, language, diags, "ja_JP", "LANG");
    free((void *)language.code);
    mnd_diag_list_free(&diags);

    ok = language_resolve(modifier_only, 1, NULL, &language, &diags);
    LANGUAGE_CHECK_OK(ok, language, diags, "sr_RS", "LC_MESSAGES");
    free((void *)language.code);
    mnd_diag_list_free(&diags);
}

/* C と POSIX は未設定として次の環境変数を見る（5.4） */
NAOS_TEST(language_skips_c_and_posix)
{
    static const test_env_var_t vars[] = {
        { "LC_ALL", "C" },
        { "LC_MESSAGES", "POSIX" },
        { "LANG", "de_DE.ISO-8859-1" },
    };
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(vars, 3, NULL, &language, &diags);

    LANGUAGE_CHECK_OK(ok, language, diags, "de_DE", "LANG");

    free((void *)language.code);
    mnd_diag_list_free(&diags);
}

/* 空の値と、除いた後に C や空になる値（C.UTF-8、.UTF-8）は未設定として扱う（5.3、5.4） */
NAOS_TEST(language_skips_empty_and_c_utf8)
{
    static const test_env_var_t empty_and_c_utf8[] = {
        { "LC_ALL", "" },
        { "LC_MESSAGES", "C.UTF-8" },
        { "LANG", "pt_BR.UTF-8" },
    };
    static const test_env_var_t empty_after_strip[] = {
        { "LC_ALL", ".UTF-8" },
        { "LC_MESSAGES", "@euro" },
        { "LANG", "it_IT" },
    };
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(empty_and_c_utf8, 3, NULL, &language, &diags);

    LANGUAGE_CHECK_OK(ok, language, diags, "pt_BR", "LANG");
    free((void *)language.code);
    mnd_diag_list_free(&diags);

    ok = language_resolve(empty_after_strip, 3, NULL, &language, &diags);
    LANGUAGE_CHECK_OK(ok, language, diags, "it_IT", "LANG");
    free((void *)language.code);
    mnd_diag_list_free(&diags);
}

/* どの環境変数からも決まらなければ --language を求めるエラー（5.5） */
NAOS_TEST(language_none_is_error)
{
    static const test_env_var_t vars[] = {
        { "LC_ALL", "C.UTF-8" },
        { "LC_MESSAGES", "" },
        { "LANG", "POSIX" },
    };
    mnd_cli_language_t language = { "x", "y" };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(vars, 3, NULL, &language, &diags);

    LANGUAGE_CHECK_ERROR(ok, language, diags, "cannot determine the language; specify --language");
    mnd_diag_list_free(&diags);

    language.code = "x";
    language.variable = "y";
    ok = language_resolve(NULL, 0, NULL, &language, &diags);
    LANGUAGE_CHECK_ERROR(ok, language, diags, "cannot determine the language; specify --language");
    mnd_diag_list_free(&diags);
}

/* --language の値が空白を含むか空ならエラーで、値と出どころを示す（5.7） */
NAOS_TEST(language_explicit_whitespace_or_empty_is_error)
{
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(NULL, 0, "ja JP", &language, &diags);

    LANGUAGE_CHECK_ERROR(ok, language, diags, "invalid language code 'ja JP' from --language");
    mnd_diag_list_free(&diags);

    ok = language_resolve(NULL, 0, "", &language, &diags);
    LANGUAGE_CHECK_ERROR(ok, language, diags, "invalid language code '' from --language");
    mnd_diag_list_free(&diags);
}

/* 環境変数の値が空白を含めばエラーで、値と変数名を示し、次の環境変数は見ない（5.7） */
NAOS_TEST(language_env_whitespace_is_error)
{
    static const test_env_var_t vars[] = {
        { "LC_ALL", "ja JP.UTF-8" },
        { "LANG", "es_ES" },
    };
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(vars, 2, NULL, &language, &diags);

    LANGUAGE_CHECK_ERROR(ok, language, diags, "invalid language code 'ja JP' from LC_ALL");
    mnd_diag_list_free(&diags);
}

/* 制御文字を含む値はエラーで、制御文字は \xHH の形で示す（5.7） */
NAOS_TEST(language_control_character_is_error)
{
    static const test_env_var_t vars[] = {
        { "LANG", "ja\x7fJP" },
    };
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(NULL, 0, "ja\tJP", &language, &diags);

    LANGUAGE_CHECK_ERROR(ok, language, diags, "invalid language code 'ja\\x09JP' from --language");
    mnd_diag_list_free(&diags);

    ok = language_resolve(vars, 1, NULL, &language, &diags);
    LANGUAGE_CHECK_ERROR(ok, language, diags, "invalid language code 'ja\\x7FJP' from LANG");
    mnd_diag_list_free(&diags);
}

/* 非 ASCII のバイトを含む値もエラーにする（Windows の getenv は ANSI のことがあるため ASCII に限る） */
NAOS_TEST(language_non_ascii_is_error)
{
    static const test_env_var_t vars[] = {
        { "LANG", "ja\xE3\x81\x82" },
    };
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok = language_resolve(vars, 1, NULL, &language, &diags);

    LANGUAGE_CHECK_ERROR(ok, language, diags, "invalid language code 'ja\\xE3\\x81\\x82' from LANG");
    mnd_diag_list_free(&diags);

    ok = language_resolve(NULL, 0, "d\xC3\xA9", &language, &diags);
    LANGUAGE_CHECK_ERROR(ok, language, diags, "invalid language code 'd\\xC3\\xA9' from --language");
    mnd_diag_list_free(&diags);
}

/* プロセスの環境変数 name を value にする（value が NULL なら削除する）。失敗なら false */
static bool language_setenv(const char *name, const char *value)
{
#if defined(_WIN32)
    char assignment[64];
    size_t name_len = strlen(name);
    size_t value_len = (value != NULL) ? strlen(value) : 0;

    if ((name_len + value_len + 2) > sizeof(assignment))
    {
        return false;
    }
    memcpy(assignment, name, name_len);
    assignment[name_len] = '=';
    if (value_len > 0)
    {
        memcpy(assignment + name_len + 1, value, value_len);
    }
    assignment[name_len + 1 + value_len] = '\0';
    return (_putenv(assignment) == 0);
#else
    if (value == NULL)
    {
        return (unsetenv(name) == 0);
    }
    return (setenv(name, value, 1) == 0);
#endif
}

/* env->getenv が NULL なら標準の getenv で引く */
NAOS_TEST(language_null_getenv_uses_process_environment)
{
    mnd_cli_env_t env;
    mnd_cli_language_t language = { NULL, NULL };
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok;

    memset(&env, 0, sizeof(env));
    env.getenv = NULL;
    NAOS_REQUIRE_TRUE(language_setenv("LC_ALL", "nl_NL.UTF-8"));

    ok = mnd_cli_resolve_language(&env, NULL, &language, &diags);
    LANGUAGE_CHECK_OK(ok, language, diags, "nl_NL", "LC_ALL");

    free((void *)language.code);
    mnd_diag_list_free(&diags);
    NAOS_CHECK_TRUE(language_setenv("LC_ALL", NULL));
}

NAOS_SUITE(language,
    NAOS_ADD_TEST(language_explicit_has_priority),
    NAOS_ADD_TEST(language_explicit_kept_as_is),
    NAOS_ADD_TEST(language_lc_all_first),
    NAOS_ADD_TEST(language_lc_messages_before_lang),
    NAOS_ADD_TEST(language_lang_last),
    NAOS_ADD_TEST(language_strips_codeset_and_modifier),
    NAOS_ADD_TEST(language_skips_c_and_posix),
    NAOS_ADD_TEST(language_skips_empty_and_c_utf8),
    NAOS_ADD_TEST(language_none_is_error),
    NAOS_ADD_TEST(language_explicit_whitespace_or_empty_is_error),
    NAOS_ADD_TEST(language_env_whitespace_is_error),
    NAOS_ADD_TEST(language_control_character_is_error),
    NAOS_ADD_TEST(language_non_ascii_is_error),
    NAOS_ADD_TEST(language_null_getenv_uses_process_environment)
)
