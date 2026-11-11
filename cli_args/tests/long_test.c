#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "test_decl.h"

/* サブコマンドの後の長いオプション（3.1〜3.7）と、サブコマンドの後の --help（8.2） */

#define LONG_MAX_ARGS 8
#define LONG_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct long_run
{
    char storage[LONG_MAX_ARGS][LONG_MAX_LEN];
    char *argv[LONG_MAX_ARGS + 1];
    clopts_result_t result;
    clopts_errors_t errors;
    clopts_status_t status;
} long_run_t;

/* tokens の argc 個を引数列として test_tool_program で解析する */
static void run_parse(long_run_t *run, int argc, const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    clopts_result_init(&run->result);
    clopts_errors_init(&run->errors);

    NAOS_REQUIRE_TRUE(argc <= LONG_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < LONG_MAX_LEN);
        strcpy(run->storage[i], tokens[i]);
        run->argv[i] = run->storage[i];
    }
    run->argv[argc] = NULL;

    run->status = clopts_parse(&test_tool_program, argc, run->argv, &run->result, &run->errors);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
}

static void run_free(long_run_t *run)
{
    clopts_result_free(&run->result);
    clopts_errors_free(&run->errors);
}

/* エラーなしで CLOPTS_OK になり、build が選択されていることを確かめる */
static void check_ok(const long_run_t *run)
{
    NAOS_CHECK_INT_EQ(CLOPTS_OK, run->status);
    NAOS_CHECK_PTR_EQ(test_tool_build, clopts_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run->errors));
}

/* 使い方のエラーで、期待するメッセージのエラーがちょうど 1 件だけ追加されたことを確かめる */
static void check_usage_error(const long_run_t *run, const char *expected)
{
    const char *message;

    NAOS_CHECK_INT_EQ(CLOPTS_USAGE_ERROR, run->status);
    NAOS_CHECK_PTR_EQ(test_tool_build, clopts_command(&run->result));
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&run->errors));
    message = clopts_errors_at(&run->errors, 0);
    NAOS_CHECK_STR_EQ(expected, message);
}

/* フラグ（3.1） */

NAOS_TEST(flag_is_set)
{
    static const char *const tokens[] = {"tool", "build", "--verbose", "--output", "y", "x"};
    long_run_t run;

    run_parse(&run, 6, tokens);
    check_ok(&run);
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "verbose"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "verbose"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "quiet"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "quiet"));
    NAOS_REQUIRE_UINT_EQ(1, clopts_positional_count(&run.result));
    NAOS_CHECK_PTR_EQ(run.argv[5], clopts_positional_at(&run.result, 0));
    run_free(&run);
}

/* 次のトークンを値として消費する（3.2） */

NAOS_TEST(value_takes_next_token)
{
    static const char *const tokens[] = {"tool", "build", "--output", "y", "x"};
    long_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    NAOS_CHECK_PTR_EQ(run.argv[3], clopts_value(&run.result, "output"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "output"));
    NAOS_REQUIRE_UINT_EQ(1, clopts_positional_count(&run.result));
    NAOS_CHECK_PTR_EQ(run.argv[4], clopts_positional_at(&run.result, 0));
    run_free(&run);
}

NAOS_TEST(value_takes_next_token_starting_with_dash)
{
    static const char *const tokens[] = {"tool", "build", "--output", "-y", "x"};
    long_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    NAOS_CHECK_PTR_EQ(run.argv[3], clopts_value(&run.result, "output"));
    NAOS_CHECK_STR_EQ("-y", clopts_value(&run.result, "output"));
    NAOS_REQUIRE_UINT_EQ(1, clopts_positional_count(&run.result));
    run_free(&run);
}

NAOS_TEST(value_takes_long_help_as_value)
{
    static const char *const tokens[] = {"tool", "build", "--output", "--help", "x"};
    long_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    NAOS_CHECK_STR_EQ("--help", clopts_value(&run.result, "output"));
    run_free(&run);
}

NAOS_TEST(value_takes_short_help_as_value)
{
    static const char *const tokens[] = {"tool", "build", "--output", "-h", "x"};
    long_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    NAOS_CHECK_STR_EQ("-h", clopts_value(&run.result, "output"));
    run_free(&run);
}

/* --name=value（3.3） */

NAOS_TEST(equals_gives_empty_value)
{
    static const char *const tokens[] = {"tool", "build", "--output=", "x"};
    long_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run);
    NAOS_CHECK_STR_EQ("", clopts_value(&run.result, "output"));
    NAOS_CHECK_PTR_EQ(run.argv[2] + 9, clopts_value(&run.result, "output"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "output"));
    NAOS_REQUIRE_UINT_EQ(1, clopts_positional_count(&run.result));
    NAOS_CHECK_PTR_EQ(run.argv[3], clopts_positional_at(&run.result, 0));
    run_free(&run);
}

NAOS_TEST(equals_value_is_everything_after_first_equals)
{
    static const char *const tokens[] = {"tool", "build", "--output=a=b", "x"};
    long_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run);
    NAOS_CHECK_STR_EQ("a=b", clopts_value(&run.result, "output"));
    run_free(&run);
}

NAOS_TEST(equals_value_does_not_consume_next_token)
{
    static const char *const tokens[] = {"tool", "build", "--level=2", "x", "--output=y"};
    long_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    NAOS_CHECK_STR_EQ("2", clopts_value(&run.result, "level"));
    NAOS_CHECK_STR_EQ("y", clopts_value(&run.result, "output"));
    NAOS_REQUIRE_UINT_EQ(1, clopts_positional_count(&run.result));
    NAOS_CHECK_PTR_EQ(run.argv[3], clopts_positional_at(&run.result, 0));
    run_free(&run);
}

/* 値の欠落（3.4） */

NAOS_TEST(trailing_value_option_requires_value)
{
    static const char *const tokens[] = {"tool", "build", "x", "--output"};
    long_run_t run;

    run_parse(&run, 4, tokens);
    check_usage_error(&run, "tool build: option '--output' requires a value");
    run_free(&run);
}

NAOS_TEST(trailing_list_option_requires_value)
{
    static const char *const tokens[] = {"tool", "build", "x", "--output", "y", "--keys"};
    long_run_t run;

    run_parse(&run, 6, tokens);
    check_usage_error(&run, "tool build: option '--keys' requires a value");
    run_free(&run);
}

/* フラグへの値（3.5） */

NAOS_TEST(flag_rejects_value)
{
    static const char *const tokens[] = {"tool", "build", "--verbose=1", "--output", "y", "x"};
    long_run_t run;

    run_parse(&run, 6, tokens);
    check_usage_error(&run, "tool build: option '--verbose' does not take a value");
    run_free(&run);
}

NAOS_TEST(flag_rejects_empty_value)
{
    static const char *const tokens[] = {"tool", "build", "--quiet=", "--output", "y", "x"};
    long_run_t run;

    run_parse(&run, 6, tokens);
    check_usage_error(&run, "tool build: option '--quiet' does not take a value");
    run_free(&run);
}

NAOS_TEST(help_rejects_value)
{
    static const char *const tokens[] = {"tool", "build", "--help=x"};
    long_run_t run;

    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool build: option '--help' does not take a value");
    run_free(&run);
}

/* 未知の長い名前（3.6、3.7） */

NAOS_TEST(prefix_is_unknown)
{
    static const char *const tokens[] = {"tool", "build", "--out", "y", "x"};
    long_run_t run;

    run_parse(&run, 5, tokens);
    check_usage_error(&run, "tool build: unknown option '--out'");
    run_free(&run);
}

NAOS_TEST(prefix_with_value_is_unknown_without_value)
{
    static const char *const tokens[] = {"tool", "build", "--out=y", "x"};
    long_run_t run;

    run_parse(&run, 4, tokens);
    check_usage_error(&run, "tool build: unknown option '--out'");
    run_free(&run);
}

NAOS_TEST(longer_name_is_unknown)
{
    static const char *const tokens[] = {"tool", "build", "--outputs", "y", "x"};
    long_run_t run;

    run_parse(&run, 5, tokens);
    check_usage_error(&run, "tool build: unknown option '--outputs'");
    run_free(&run);
}

NAOS_TEST(name_is_case_sensitive)
{
    static const char *const tokens[] = {"tool", "build", "--Output", "y", "x"};
    long_run_t run;

    run_parse(&run, 5, tokens);
    check_usage_error(&run, "tool build: unknown option '--Output'");
    run_free(&run);
}

NAOS_TEST(option_of_other_command_is_unknown)
{
    static const char *const tokens[] = {"tool", "cat", "a", "--output", "y"};
    long_run_t run;

    run_parse(&run, 5, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_USAGE_ERROR, run.status);
    NAOS_CHECK_PTR_EQ(test_tool_cat, clopts_command(&run.result));
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&run.errors));
    NAOS_CHECK_STR_EQ("tool cat: unknown option '--output'",
                      clopts_errors_at(&run.errors, 0));
    run_free(&run);
}

/* サブコマンドの後の --help（8.2） */

NAOS_TEST(help_after_command_requests_command_help)
{
    static const char *const tokens[] = {"tool", "build", "--help"};
    long_run_t run;

    run_parse(&run, 3, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_HELP, run.status);
    NAOS_CHECK_PTR_EQ(test_tool_build, clopts_command(&run.result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run.errors));
    run_free(&run);
}

NAOS_TEST(help_after_options_requests_command_help)
{
    static const char *const tokens[] = {"tool", "build", "--verbose", "x", "--help", "--bad"};
    long_run_t run;

    run_parse(&run, 6, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_HELP, run.status);
    NAOS_CHECK_PTR_EQ(test_tool_build, clopts_command(&run.result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run.errors));
    run_free(&run);
}

NAOS_TEST(help_as_value_is_not_help)
{
    static const char *const tokens[] = {"tool", "build", "x", "--output", "--help"};
    long_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run);
    NAOS_CHECK_PTR_EQ(run.argv[4], clopts_value(&run.result, "output"));
    run_free(&run);
}

NAOS_SUITE(long,
    NAOS_ADD_TEST(flag_is_set),
    NAOS_ADD_TEST(value_takes_next_token),
    NAOS_ADD_TEST(value_takes_next_token_starting_with_dash),
    NAOS_ADD_TEST(value_takes_long_help_as_value),
    NAOS_ADD_TEST(value_takes_short_help_as_value),
    NAOS_ADD_TEST(equals_gives_empty_value),
    NAOS_ADD_TEST(equals_value_is_everything_after_first_equals),
    NAOS_ADD_TEST(equals_value_does_not_consume_next_token),
    NAOS_ADD_TEST(trailing_value_option_requires_value),
    NAOS_ADD_TEST(trailing_list_option_requires_value),
    NAOS_ADD_TEST(flag_rejects_value),
    NAOS_ADD_TEST(flag_rejects_empty_value),
    NAOS_ADD_TEST(help_rejects_value),
    NAOS_ADD_TEST(prefix_is_unknown),
    NAOS_ADD_TEST(prefix_with_value_is_unknown_without_value),
    NAOS_ADD_TEST(longer_name_is_unknown),
    NAOS_ADD_TEST(name_is_case_sensitive),
    NAOS_ADD_TEST(option_of_other_command_is_unknown),
    NAOS_ADD_TEST(help_after_command_requests_command_help),
    NAOS_ADD_TEST(help_after_options_requests_command_help),
    NAOS_ADD_TEST(help_as_value_is_not_help)
)
