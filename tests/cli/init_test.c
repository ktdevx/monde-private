/* スイート init: Language と PO-Revision-Date、fuzzy の除去、言語の出どころ、--quiet、読み込みエラー（5.1、5.6、5.8〜5.10、8.2） */

/* POSIX の setenv / unsetenv / tzset を C99 の厳密なモードで宣言させる（Windows は _putenv / _tzset を使う） */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif

#include <naos/naos.h>

#include "cli/cli.h"
#include "foundation/buf.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 2026-01-01 12:34:00 UTC の time_t の値（UNIX 時刻） */
#define INIT_TIME ((time_t)1767225600 + (12 * 3600) + (34 * 60))

/* INIT_TIME を TZ = JST-9 で表した PO-Revision-Date（実装とは別に手で求めた値） */
#define INIT_DATE "2026-01-01 21:34+0900"

/* extract が作る形のテンプレートのヘッダ（fuzzy 付き、Language は空） */
#define INIT_TEMPLATE_HEADER \
    "#, fuzzy\n" \
    "msgid \"\"\n" \
    "msgstr \"\"\n" \
    "\"Project-Id-Version: PACKAGE VERSION\\n\"\n" \
    "\"Report-Msgid-Bugs-To: \\n\"\n" \
    "\"POT-Creation-Date: 2025-12-31 10:00+0900\\n\"\n" \
    "\"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n\"\n" \
    "\"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n\"\n" \
    "\"Language-Team: LANGUAGE <LL@li.org>\\n\"\n" \
    "\"Language: \\n\"\n" \
    "\"MIME-Version: 1.0\\n\"\n" \
    "\"Content-Type: text/plain; charset=UTF-8\\n\"\n" \
    "\"Content-Transfer-Encoding: 8bit\\n\"\n"

/* 2 件のエントリを持つテンプレート */
#define INIT_TEMPLATE \
    INIT_TEMPLATE_HEADER \
    "\n" \
    "#. heading (level 1)\n" \
    "#: doc.md:1\n" \
    "msgid \"Heading\"\n" \
    "msgstr \"\"\n" \
    "\n" \
    "#. paragraph\n" \
    "#: doc.md:3\n" \
    "msgid \"Body text.\"\n" \
    "msgstr \"\"\n"

/* ---- 補助 ---- */

/* TZ を変えたかどうかと、変える前の値（teardown で戻す） */
static bool tz_changed = false;
static bool tz_had_value = false;
static char tz_saved[256];

/* プロセスの TZ を value にしてタイムゾーンの情報を読み直す。失敗したら false */
static bool set_tz(const char *value)
{
    if (!tz_changed)
    {
        const char *current = getenv("TZ");

        tz_had_value = (current != NULL);
        if (tz_had_value)
        {
            if (strlen(current) >= sizeof(tz_saved))
            {
                return false;
            }
            strcpy(tz_saved, current);
        }
        tz_changed = true;
    }
#if defined(_WIN32)
    {
        char assignment[64];

        if ((strlen(value) + 4) > sizeof(assignment))
        {
            return false;
        }
        strcpy(assignment, "TZ=");
        strcat(assignment, value);
        if (_putenv(assignment) != 0)
        {
            return false;
        }
        _tzset();
    }
#else
    if (setenv("TZ", value, 1) != 0)
    {
        return false;
    }
    tzset();
#endif
    return true;
}

/* TZ を変えたテストの後で元の値（なければ未設定）に戻し、環境変数の表を空にする */
NAOS_TEARDOWN(init_restore_env)
{
    test_env_set(NULL, 0);
    if (!tz_changed)
    {
        return;
    }
    tz_changed = false;
#if defined(_WIN32)
    {
        char assignment[sizeof(tz_saved) + 4];

        strcpy(assignment, "TZ=");
        if (tz_had_value)
        {
            strcat(assignment, tz_saved);
        }
        (void)_putenv(assignment);
        _tzset();
    }
#else
    if (tz_had_value)
    {
        (void)setenv("TZ", tz_saved, 1);
    }
    else
    {
        (void)unsetenv("TZ");
    }
    tzset();
#endif
}

#define INIT_MAX_ARGS 16
#define INIT_MAX_LEN 1024

typedef struct init_argv
{
    char storage[INIT_MAX_ARGS][INIT_MAX_LEN];
    char *argv[INIT_MAX_ARGS + 1];
    int argc;
} init_argv_t;

/*
 * "monde init" に NULL で終わる args を続けて test_cli_run で実行する（name はテストごとに一意）。
 * 環境変数の表は vars の count 行（NULL / 0 なら空の表）、時計は INIT_TIME、TZ は JST-9 にする
 */
static void run_init(const char *name, const char *stdin_bytes, size_t stdin_len, const char *const *args,
                     const test_env_var_t *vars, size_t count, test_cli_result_t *result)
{
    static init_argv_t copy;
    size_t i;

    NAOS_REQUIRE_TRUE(set_tz("JST-9"));
    test_clock_set(INIT_TIME);
    test_env_set(vars, count);

    strcpy(copy.storage[0], "monde");
    strcpy(copy.storage[1], "init");
    copy.argc = 2;
    for (i = 0; args[i] != NULL; i++)
    {
        NAOS_REQUIRE_TRUE(copy.argc < INIT_MAX_ARGS);
        NAOS_REQUIRE_TRUE(strlen(args[i]) < INIT_MAX_LEN);
        strcpy(copy.storage[copy.argc], args[i]);
        copy.argc++;
    }
    for (i = 0; i < (size_t)copy.argc; i++)
    {
        copy.argv[i] = copy.storage[i];
    }
    copy.argv[copy.argc] = NULL;

    NAOS_REQUIRE_TRUE(test_cli_run(name, stdin_bytes, stdin_len, copy.argc, copy.argv, result));
}

/* 作業ディレクトリに name の内容 text（strlen の長さ）を書き、そのパスを path に入れる */
static void write_input(const char *name, const char *text, mnd_buf_t *path)
{
    NAOS_REQUIRE_TRUE(test_write_file(name, text, strlen(text)));
    NAOS_REQUIRE_TRUE(test_work_path(name, path));
}

/* buf の内容が needle を含む */
static bool buf_contains(const mnd_buf_t *buf, const char *needle)
{
    return (strstr(mnd_buf_cstr(buf), needle) != NULL);
}

/* buf のすべての \n の直前が \r である（\n が 1 つ以上ある） */
static bool all_lines_crlf(const mnd_buf_t *buf)
{
    const char *data = mnd_buf_cstr(buf);
    size_t newlines = 0;
    size_t i;

    for (i = 0; i < buf->len; i++)
    {
        if (data[i] == '\n')
        {
            if ((i == 0) || (data[i - 1] != '\r'))
            {
                return false;
            }
            newlines++;
        }
    }
    return (newlines > 0);
}

/*
 * 不正なテンプレート text をファイル name.pot に書いて -l ja -o name.po で init を実行し、
 * 終了コード 1、出力ファイルなし、標準出力は空、err がちょうど「<path><suffix>」の行の並びであることを確かめる。
 * suffixes は NULL で終わる、各行のパスの後ろの部分（":<line>: error: <message>"）
 */
static void check_invalid_template(const char *name, const char *text, const char *const *suffixes)
{
    char template_name[128];
    char output_name[128];
    const char *args[6];
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t output_path = MND_BUF_INIT;
    mnd_buf_t expected = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;
    size_t i;

    NAOS_REQUIRE_TRUE(strlen(name) + 4 < sizeof(template_name));
    strcpy(template_name, name);
    strcat(template_name, ".pot");
    strcpy(output_name, name);
    strcat(output_name, ".po");

    write_input(template_name, text, &template_path);
    NAOS_REQUIRE_TRUE(test_remove_file(output_name));
    NAOS_REQUIRE_TRUE(test_work_path(output_name, &output_path));
    for (i = 0; suffixes[i] != NULL; i++)
    {
        NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected, mnd_buf_cstr(&template_path))
                          && mnd_buf_append_str(&expected, suffixes[i])
                          && mnd_buf_append_str(&expected, "\n"));
    }

    args[0] = "-l";
    args[1] = "ja";
    args[2] = "-o";
    args[3] = mnd_buf_cstr(&output_path);
    args[4] = mnd_buf_cstr(&template_path);
    args[5] = NULL;
    run_init(name, NULL, 0, args, NULL, 0, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_FALSE(test_file_exists(output_name));
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&expected), mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&expected);
    mnd_buf_free(&output_path);
    mnd_buf_free(&template_path);
}

/* ---- テスト ---- */

/*
 * -l の値が Language に、固定時刻が PO-Revision-Date に入り、ヘッダの fuzzy が外れ、エントリは msgstr が空のまま
 * 引き継がれる。出どころの表示はなく、件数と言語を表示する（5.1、5.2、5.8、8.2）
 */
NAOS_TEST(init_explicit_language_sets_header)
{
    static const char input[] = INIT_TEMPLATE;
    static const char *const args[] = { "-l", "ja", "-", NULL };
    static const char expected[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Project-Id-Version: PACKAGE VERSION\\n\"\n"
        "\"Report-Msgid-Bugs-To: \\n\"\n"
        "\"POT-Creation-Date: 2025-12-31 10:00+0900\\n\"\n"
        "\"PO-Revision-Date: " INIT_DATE "\\n\"\n"
        "\"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n\"\n"
        "\"Language-Team: LANGUAGE <LL@li.org>\\n\"\n"
        "\"Language: ja\\n\"\n"
        "\"MIME-Version: 1.0\\n\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"Content-Transfer-Encoding: 8bit\\n\"\n"
        "\n"
        "#. heading (level 1)\n"
        "#: doc.md:1\n"
        "msgid \"Heading\"\n"
        "msgstr \"\"\n"
        "\n"
        "#. paragraph\n"
        "#: doc.md:3\n"
        "msgid \"Body text.\"\n"
        "msgstr \"\"\n";
    static const test_env_var_t vars[] = { { "LANG", "fr_FR.UTF-8" } };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    /* 環境変数があっても -l が優先され、出どころは表示しない */
    run_init("init_explicit_language_sets_header", input, strlen(input), args, vars, 1, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ(expected, mnd_buf_cstr(&result.out));
    NAOS_CHECK_STR_EQ("init: 2 messages, language ja\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
}

/* 環境変数から決めたときは出どころを、続けて件数と言語を表示する。-o のファイルに書く（5.3、5.6、8.2） */
NAOS_TEST(init_language_from_environment_reports_source)
{
    static const char text[] = INIT_TEMPLATE;
    static const test_env_var_t vars[] = { { "LC_ALL", "" }, { "LC_MESSAGES", "C" }, { "LANG", "ja_JP.UTF-8" } };
    const char *args[4];
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t output_path = MND_BUF_INIT;
    mnd_buf_t written = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("init_language_from_environment_reports_source.pot", text, &template_path);
    NAOS_REQUIRE_TRUE(test_remove_file("init_language_from_environment_reports_source.po"));
    NAOS_REQUIRE_TRUE(test_work_path("init_language_from_environment_reports_source.po", &output_path));

    args[0] = "-o";
    args[1] = mnd_buf_cstr(&output_path);
    args[2] = mnd_buf_cstr(&template_path);
    args[3] = NULL;
    run_init("init_language_from_environment_reports_source", NULL, 0, args, vars, 3, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ("init: using language 'ja_JP' from LANG\n"
                      "init: 2 messages, language ja_JP\n",
                      mnd_buf_cstr(&result.err));
    NAOS_REQUIRE_TRUE(test_read_file("init_language_from_environment_reports_source.po", &written));
    NAOS_CHECK_TRUE(buf_contains(&written, "\"Language: ja_JP\\n\"\n"));
    NAOS_CHECK_TRUE(buf_contains(&written, "\"PO-Revision-Date: " INIT_DATE "\\n\"\n"));
    NAOS_CHECK_FALSE(buf_contains(&written, "fuzzy"));
    NAOS_CHECK_TRUE(buf_contains(&written, "msgid \"Body text.\"\nmsgstr \"\"\n"));

    test_cli_result_free(&result);
    mnd_buf_free(&written);
    mnd_buf_free(&output_path);
    mnd_buf_free(&template_path);
}

/* --quiet なら出どころも件数も表示しない（5.6、8.7） */
NAOS_TEST(init_quiet_suppresses_reports)
{
    static const char input[] = INIT_TEMPLATE;
    static const char *const args[] = { "--quiet", "-", NULL };
    static const test_env_var_t vars[] = { { "LC_MESSAGES", "de_DE@euro" } };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_init("init_quiet_suppresses_reports", input, strlen(input), args, vars, 1, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.err.len);
    NAOS_CHECK_TRUE(buf_contains(&result.out, "\"Language: de_DE\\n\"\n"));
    test_cli_result_free(&result);
}

/* 言語が決まらなければ --language を求める使い方のエラーを表示し、出力を作らずに 2 で終わる（5.5） */
NAOS_TEST(init_language_undetermined_is_usage_error)
{
    static const char text[] = INIT_TEMPLATE;
    const char *args[4];
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t output_path = MND_BUF_INIT;
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("init_language_undetermined_is_usage_error.pot", text, &template_path);
    NAOS_REQUIRE_TRUE(test_remove_file("init_language_undetermined_is_usage_error.po"));
    NAOS_REQUIRE_TRUE(test_work_path("init_language_undetermined_is_usage_error.po", &output_path));

    args[0] = "-o";
    args[1] = mnd_buf_cstr(&output_path);
    args[2] = mnd_buf_cstr(&template_path);
    args[3] = NULL;
    /* 環境変数の表は空 */
    run_init("init_language_undetermined_is_usage_error", NULL, 0, args, NULL, 0, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
    NAOS_CHECK_FALSE(test_file_exists("init_language_undetermined_is_usage_error.po"));
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ("monde: error: cannot determine the language; specify --language\n",
                      mnd_buf_cstr(&result.err));

    test_cli_result_free(&result);
    mnd_buf_free(&output_path);
    mnd_buf_free(&template_path);
}

/* 空白を含む言語コードは値と出どころを示す使い方のエラーで、出力を作らずに 2 で終わる（5.7） */
NAOS_TEST(init_invalid_language_is_usage_error)
{
    static const char input[] = INIT_TEMPLATE;
    static const char *const args[] = { "-l", "j a", "-", NULL };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_init("init_invalid_language_is_usage_error", input, strlen(input), args, NULL, 0, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ("monde: error: invalid language code 'j a' from --language\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
}

/* 長い msgid: 既定では折り返し、--no-wrap では折り返さない（5.9） */
NAOS_TEST(init_no_wrap)
{
    static const char input[] =
        INIT_TEMPLATE_HEADER
        "\n"
        "msgid \"This paragraph is intentionally long so that the default writer has to wrap it across lines.\"\n"
        "msgstr \"\"\n";
    static const char *const wrap_args[] = { "-l", "ja", "-", NULL };
    static const char *const no_wrap_args[] = { "-l", "ja", "--no-wrap", "-", NULL };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_init("init_no_wrap_default", input, strlen(input), wrap_args, NULL, 0, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(buf_contains(&result.out, "msgid \"\"\n\"This paragraph is intentionally long "));

    run_init("init_no_wrap_enabled", input, strlen(input), no_wrap_args, NULL, 0, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(buf_contains(&result.out,
        "msgid \"This paragraph is intentionally long so that the default writer has to wrap it across lines.\"\n"));
    test_cli_result_free(&result);
}

/* CRLF で BOM 付きのテンプレート: 出力は CRLF で BOM なし（3.3、3.7） */
NAOS_TEST(init_crlf_bom_template)
{
    static const char input[] =
        "\xEF\xBB\xBF"
        "msgid \"\"\r\n"
        "msgstr \"\"\r\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\r\n"
        "\r\n"
        "msgid \"Body.\"\r\n"
        "msgstr \"\"\r\n";
    static const char *const args[] = { "-l", "ja", "-", NULL };
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_init("init_crlf_bom_template", input, strlen(input), args, NULL, 0, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_TRUE(all_lines_crlf(&result.out));
    NAOS_CHECK_FALSE(buf_contains(&result.out, "\xEF\xBB\xBF"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "\"Language: ja\\n\"\r\n"));
    NAOS_CHECK_TRUE(buf_contains(&result.out, "msgid \"Body.\"\r\nmsgstr \"\"\r\n"));
    NAOS_CHECK_STR_EQ("init: 1 messages, language ja\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
}

/* 構文エラーのテンプレートはパスと行のエラーを表示して 1 で終わる（5.10） */
NAOS_TEST(init_invalid_template_syntax)
{
    static const char text[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgid \"Body.\"\n"
        "bogus line\n";
    mnd_buf_t template_path = MND_BUF_INIT;
    mnd_buf_t output_path = MND_BUF_INIT;
    mnd_buf_t expected_prefix = MND_BUF_INIT;
    const char *args[6];
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    write_input("init_invalid_template_syntax.pot", text, &template_path);
    NAOS_REQUIRE_TRUE(test_remove_file("init_invalid_template_syntax.po"));
    NAOS_REQUIRE_TRUE(test_work_path("init_invalid_template_syntax.po", &output_path));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&expected_prefix, mnd_buf_cstr(&template_path))
                      && mnd_buf_append_str(&expected_prefix, ":6: error: "));

    args[0] = "-l";
    args[1] = "ja";
    args[2] = "-o";
    args[3] = mnd_buf_cstr(&output_path);
    args[4] = mnd_buf_cstr(&template_path);
    args[5] = NULL;
    run_init("init_invalid_template_syntax", NULL, 0, args, NULL, 0, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_FALSE(test_file_exists("init_invalid_template_syntax.po"));
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_REQUIRE_UINT_GE(result.err.len, expected_prefix.len);
    NAOS_CHECK_MEM_EQ(mnd_buf_cstr(&expected_prefix), expected_prefix.len, mnd_buf_cstr(&result.err),
                      expected_prefix.len);
    NAOS_CHECK_FALSE(buf_contains(&result.err, "init:"));

    test_cli_result_free(&result);
    mnd_buf_free(&expected_prefix);
    mnd_buf_free(&output_path);
    mnd_buf_free(&template_path);
}

/* UTF-8 以外の charset と 2 組の重複: 報告されたすべてのエラーを報告の順に表示し、出力を作らずに 1 で終わる（5.10） */
NAOS_TEST(init_invalid_template_reports_all_errors)
{
    static const char text[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=ISO-8859-1\\n\"\n"
        "\n"
        "msgid \"A\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"A\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"B\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgid \"B\"\n"
        "msgstr \"\"\n";
    static const char *const suffixes[] = {
        ":8: error: duplicate message definition (first defined at line 5)",
        ":14: error: duplicate message definition (first defined at line 11)",
        ":1: error: unsupported charset 'ISO-8859-1'; only UTF-8 is supported",
        NULL
    };

    check_invalid_template("init_invalid_template_reports_all_errors", text, suffixes);
}

/* 重複だけのテンプレート（5.10） */
NAOS_TEST(init_invalid_template_duplicate)
{
    static const char text[] =
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\n"
        "msgctxt \"c\"\n"
        "msgid \"A\"\n"
        "msgstr \"\"\n"
        "\n"
        "msgctxt \"c\"\n"
        "msgid \"A\"\n"
        "msgstr \"\"\n";
    static const char *const suffixes[] = {
        ":9: error: duplicate message definition (first defined at line 5)",
        NULL
    };

    check_invalid_template("init_invalid_template_duplicate", text, suffixes);
}

NAOS_SUITE(init,
    NAOS_SET_TEARDOWN(init_restore_env),
    NAOS_ADD_TEST(init_explicit_language_sets_header),
    NAOS_ADD_TEST(init_language_from_environment_reports_source),
    NAOS_ADD_TEST(init_quiet_suppresses_reports),
    NAOS_ADD_TEST(init_language_undetermined_is_usage_error),
    NAOS_ADD_TEST(init_invalid_language_is_usage_error),
    NAOS_ADD_TEST(init_no_wrap),
    NAOS_ADD_TEST(init_crlf_bom_template),
    NAOS_ADD_TEST(init_invalid_template_syntax),
    NAOS_ADD_TEST(init_invalid_template_reports_all_errors),
    NAOS_ADD_TEST(init_invalid_template_duplicate)
)
