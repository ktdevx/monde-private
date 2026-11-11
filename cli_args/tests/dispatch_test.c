#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "test_decl.h"

/* ルートコマンドから始まる走査での子コマンドの選択（2.x、8.x）と、解析の入口（1.6、7.5、7.6） */

#define DISPATCH_MAX_ARGS 8
#define DISPATCH_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct dispatch_run
{
    char storage[DISPATCH_MAX_ARGS][DISPATCH_MAX_LEN];
    char *argv[DISPATCH_MAX_ARGS + 1];
    clopts_result_t result;
    clopts_errors_t errors;
    clopts_status_t status;
} dispatch_run_t;

static void run_init(dispatch_run_t *run)
{
    memset(run, 0, sizeof *run);
    clopts_result_init(&run->result);
    clopts_errors_init(&run->errors);
}

/* tokens の argc 個を引数列として解析する。run の結果とエラーは前回の内容に上書き・追加される */
static void run_parse_with(dispatch_run_t *run, const clopts_program_t *program, int argc,
                           const char *const *tokens)
{
    int i;

    NAOS_REQUIRE_TRUE(argc <= DISPATCH_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < DISPATCH_MAX_LEN);
        strcpy(run->storage[i], tokens[i]);
        run->argv[i] = run->storage[i];
    }
    run->argv[argc] = NULL;

    run->status = clopts_parse(program, argc, run->argv, &run->result, &run->errors);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
}

static void run_parse(dispatch_run_t *run, int argc, const char *const *tokens)
{
    run_parse_with(run, &test_tool_program, argc, tokens);
}

static void run_free(dispatch_run_t *run)
{
    clopts_result_free(&run->result);
    clopts_errors_free(&run->errors);
}

/* 使い方のエラーで、期待するメッセージのエラーがちょうど 1 件だけ追加されたことを確かめる（7.4、7.5） */
static void check_usage_error(const dispatch_run_t *run, const char *expected)
{
    const char *message;

    NAOS_CHECK_INT_EQ(CLOPTS_USAGE_ERROR, run->status);
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&run->errors));
    message = clopts_errors_at(&run->errors, 0);
    NAOS_CHECK_STR_EQ(expected, message);
}

/*
 * 経路を確かめる。command が NULL ならルートコマンドだけの 1 段、
 * そうでなければルートコマンドと command の 2 段（2.8）
 */
static void check_path(const clopts_result_t *result, const clopts_command_t *command)
{
    if (command == NULL)
    {
        NAOS_CHECK_UINT_EQ(1, clopts_depth(result));
        NAOS_CHECK_PTR_EQ(test_tool_program.root, clopts_command(result));
    }
    else
    {
        NAOS_CHECK_UINT_EQ(2, clopts_depth(result));
        NAOS_CHECK_PTR_EQ(command, clopts_command(result));
    }
    NAOS_CHECK_PTR_EQ(test_tool_program.root, clopts_command_at(result, 0));
}

/* 選択中のコマンド（最後の段）の値の配列 */
static const clopts_value_t *selected_values(const clopts_result_t *result)
{
    NAOS_REQUIRE_UINT_GT(result->level_count, 0);
    return result->levels[result->level_count - 1].values;
}

/* エラーなしで status になり、経路が check_path のとおりであることを確かめる */
static void check_status(const dispatch_run_t *run, clopts_status_t status,
                         const clopts_command_t *command)
{
    NAOS_CHECK_INT_EQ(status, run->status);
    check_path(&run->result, command);
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run->errors));
}

/* 宣言済みのサブコマンドの選択（2.1） */

NAOS_TEST(build_is_selected)
{
    static const char *const tokens[] = {"tool", "build", "x", "-o", "y"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 5, tokens);
    check_status(&run, CLOPTS_OK, test_tool_build);
    run_free(&run);
}

NAOS_TEST(cat_is_selected_with_positionals)
{
    static const char *const tokens[] = {"tool", "cat", "a", "b"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_status(&run, CLOPTS_OK, test_tool_cat);
    NAOS_REQUIRE_UINT_EQ(2, clopts_positional_count(&run.result));
    NAOS_CHECK_PTR_EQ(run.argv[2], clopts_positional_at(&run.result, 0));
    NAOS_CHECK_PTR_EQ(run.argv[3], clopts_positional_at(&run.result, 1));
    run_free(&run);
}

NAOS_TEST(copy_is_selected)
{
    static const char *const tokens[] = {"tool", "copy", "a", "b"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_status(&run, CLOPTS_OK, test_tool_copy);
    run_free(&run);
}

/* 未知のサブコマンド（2.2、2.4） */

NAOS_TEST(misspelled_command_is_unknown)
{
    static const char *const tokens[] = {"tool", "bulid", "x"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool: unknown command 'bulid'");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(command_name_is_case_sensitive)
{
    static const char *const tokens[] = {"tool", "Build"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_usage_error(&run, "tool: unknown command 'Build'");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(command_prefix_is_unknown)
{
    static const char *const tokens[] = {"tool", "bui"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_usage_error(&run, "tool: unknown command 'bui'");
    run_free(&run);
}

NAOS_TEST(single_dash_is_unknown_command)
{
    static const char *const tokens[] = {"tool", "-"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_usage_error(&run, "tool: unknown command '-'");
    check_path(&run.result, NULL);
    run_free(&run);
}

/* サブコマンドより前の未知のオプション（2.3） */

NAOS_TEST(option_before_command_is_unknown)
{
    static const char *const tokens[] = {"tool", "--verbose", "build"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool: unknown option '--verbose'");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(short_option_before_command_is_unknown)
{
    static const char *const tokens[] = {"tool", "-v", "build"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool: unknown option '-v'");
    run_free(&run);
}

/* オプションの形の規則は子コマンドの選択前にも同じように適用する（3.5、3.6、4.5） */

NAOS_TEST(unknown_long_option_with_value_shows_name_only)
{
    static const char *const tokens[] = {"tool", "--foo=bar"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_usage_error(&run, "tool: unknown option '--foo'");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(help_with_value_takes_no_value)
{
    static const char *const tokens[] = {"tool", "--help=1"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_usage_error(&run, "tool: option '--help' does not take a value");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(version_with_value_takes_no_value)
{
    static const char *const tokens[] = {"tool", "--version=1"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_usage_error(&run, "tool: option '--version' does not take a value");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(short_option_cluster_before_command_shows_first_letter)
{
    static const char *const tokens[] = {"tool", "-xy", "build"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool: unknown option '-x'");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(non_ascii_short_option_before_command_is_unknown)
{
    static const char *const tokens[] = {"tool", "-\xE6\x97\xA5"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_usage_error(&run, "tool: unknown option '-\xE6\x97\xA5'");
    check_path(&run.result, NULL);
    run_free(&run);
}

/* 子を持つコマンドでの "--"（2.4、2.5） */

NAOS_TEST(double_dash_then_command_is_unexpected)
{
    static const char *const tokens[] = {"tool", "--", "build"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool: unexpected argument 'build'");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(double_dash_alone_is_no_command)
{
    static const char *const tokens[] = {"tool", "--"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_status(&run, CLOPTS_NO_COMMAND, NULL);
    run_free(&run);
}

/* 引数なし（8.6） */

NAOS_TEST(no_arguments_is_no_command)
{
    static const char *const tokens[] = {"tool"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 1, tokens);
    check_status(&run, CLOPTS_NO_COMMAND, NULL);
    run_free(&run);
}

NAOS_TEST(argc_zero_is_no_command)
{
    char *argv[] = {NULL};
    clopts_result_t result = CLOPTS_RESULT_INIT;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    NAOS_CHECK_INT_EQ(CLOPTS_NO_COMMAND, clopts_parse(&test_tool_program, 0, argv, &result, &errors));
    NAOS_CHECK_INT_EQ(CLOPTS_NO_COMMAND, result.status);
    check_path(&result, NULL);
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&errors));
    clopts_result_free(&result);
    clopts_errors_free(&errors);
}

/* ヘルプとバージョン（8.1、8.3、8.4、8.5） */

NAOS_TEST(long_help_ignores_later_tokens)
{
    static const char *const tokens[] = {"tool", "--help", "bulid", "--bad"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_status(&run, CLOPTS_HELP, NULL);
    run_free(&run);
}

NAOS_TEST(short_help_ignores_later_tokens)
{
    static const char *const tokens[] = {"tool", "-h", "build", "--bad"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_status(&run, CLOPTS_HELP, NULL);
    run_free(&run);
}

NAOS_TEST(help_word_alone_is_program_help)
{
    static const char *const tokens[] = {"tool", "help"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_status(&run, CLOPTS_HELP, NULL);
    run_free(&run);
}

NAOS_TEST(help_word_with_command_is_command_help)
{
    static const char *const tokens[] = {"tool", "help", "copy"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_status(&run, CLOPTS_HELP, test_tool_copy);
    NAOS_CHECK_NULL(selected_values(&run.result));
    NAOS_CHECK_UINT_EQ(0, clopts_positional_count(&run.result));
    run_free(&run);
}

NAOS_TEST(help_word_with_unknown_command)
{
    static const char *const tokens[] = {"tool", "help", "nope"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool: unknown command 'nope'");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(help_word_with_two_arguments)
{
    static const char *const tokens[] = {"tool", "help", "a", "b"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_usage_error(&run, "tool: unknown command 'a'");
    check_path(&run.result, NULL);
    run_free(&run);
}

/*
 * 末端コマンドに達した後の引数は余分な引数とし、段を help を書いたコマンドまで戻す（8.6）
 */
NAOS_TEST(help_word_with_two_known_commands)
{
    static const char *const tokens[] = {"tool", "help", "copy", "build"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_usage_error(&run, "tool: unexpected argument 'build'");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(help_word_with_command_and_extra_argument)
{
    static const char *const tokens[] = {"tool", "help", "copy", "x"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_usage_error(&run, "tool: unexpected argument 'x'");
    check_path(&run.result, NULL);
    run_free(&run);
}

/* help の後のトークンはすべて名前として扱い、オプションとして解釈しない（8.6） */
NAOS_TEST(help_word_treats_option_like_token_as_name)
{
    static const char *const tokens[] = {"tool", "help", "--verbose"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool: unknown command '--verbose'");
    check_path(&run.result, NULL);
    run_free(&run);
}

NAOS_TEST(version_ignores_later_tokens)
{
    static const char *const tokens[] = {"tool", "--version", "bulid"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_status(&run, CLOPTS_VERSION, NULL);
    run_free(&run);
}

NAOS_TEST(version_with_extra_argument_is_version)
{
    static const char *const tokens[] = {"tool", "--version", "x"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_status(&run, CLOPTS_VERSION, NULL);
    run_free(&run);
}

NAOS_TEST(help_before_command_is_root_help)
{
    static const char *const tokens[] = {"tool", "--help", "build"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_status(&run, CLOPTS_HELP, NULL);
    run_free(&run);
}

/* 子コマンドの選択後の --version は未知のオプション（8.9） */

NAOS_TEST(version_after_command_is_unknown_option)
{
    static const char *const tokens[] = {"tool", "build", "--version"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool build: unknown option '--version'");
    check_path(&run.result, test_tool_build);
    run_free(&run);
}

NAOS_TEST(version_with_value_after_command_is_unknown_option)
{
    static const char *const tokens[] = {"tool", "build", "--version=1"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool build: unknown option '--version'");
    check_path(&run.result, test_tool_build);
    run_free(&run);
}

/* 宣言の誤り（1.6）: 引数列を読まずに返す */

static const clopts_command_t dispatch_bad_commands[] = {
    { .name = "help", .summary = "Show help" }
};

static const clopts_command_t dispatch_bad_root = {
    .commands = dispatch_bad_commands,
    .command_count = 1
};

static const clopts_program_t dispatch_bad_program = {
    .name = "bad",
    .version = "0.1",
    .root = &dispatch_bad_root
};

NAOS_TEST(invalid_declaration_is_decl_error_without_reading_argv)
{
    /* 要素を読めばクラッシュする引数列。宣言の誤りでは読まれないことを確かめる */
    char *argv[] = {NULL, NULL, NULL, NULL};
    clopts_result_t result = CLOPTS_RESULT_INIT;
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    size_t i;

    NAOS_CHECK_INT_EQ(CLOPTS_DECL_ERROR,
                      clopts_parse(&dispatch_bad_program, 3, argv, &result, &errors));
    NAOS_CHECK_INT_EQ(CLOPTS_DECL_ERROR, result.status);
    NAOS_CHECK_NULL(clopts_command(&result));
    NAOS_CHECK_UINT_EQ(0, clopts_depth(&result));
    NAOS_REQUIRE_UINT_GT(clopts_errors_count(&errors), 0);
    for (i = 0; i < clopts_errors_count(&errors); i++)
    {
        const char *message = clopts_errors_at(&errors, i);

        NAOS_CHECK_TRUE(strncmp(message, "invalid argument declaration: ",
                                strlen("invalid argument declaration: ")) == 0);
    }
    clopts_result_free(&result);
    clopts_errors_free(&errors);
}

NAOS_TEST(invalid_declaration_with_tokens)
{
    static const char *const tokens[] = {"bad", "--help"};
    dispatch_run_t run;

    run_init(&run);
    run_parse_with(&run, &dispatch_bad_program, 2, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_DECL_ERROR, run.status);
    NAOS_CHECK_UINT_GT(clopts_errors_count(&run.errors), 0);
    run_free(&run);
}

/* 同じ結果の再利用: 以前の内容を解放してから書き込む */

NAOS_TEST(result_is_reset_between_parses)
{
    static const char *const first[] = {"tool", "build", "x"};
    static const char *const second[] = {"tool", "cat", "a", "b", "c"};
    static const char *const third[] = {"tool", "cat", "d"};
    static const char *const fourth[] = {"tool", "--help"};
    dispatch_run_t run;

    run_init(&run);

    /* build は値の領域を確保する */
    run_parse(&run, 3, first);
    check_path(&run.result, test_tool_build);
    NAOS_CHECK_NOT_NULL(selected_values(&run.result));
    clopts_errors_free(&run.errors);

    run_parse(&run, 5, second);
    check_status(&run, CLOPTS_OK, test_tool_cat);
    NAOS_CHECK_NULL(selected_values(&run.result));
    NAOS_CHECK_UINT_EQ(3, clopts_positional_count(&run.result));

    /* 前回の位置引数は残らない */
    run_parse(&run, 3, third);
    check_status(&run, CLOPTS_OK, test_tool_cat);
    NAOS_REQUIRE_UINT_EQ(1, clopts_positional_count(&run.result));
    NAOS_CHECK_STR_EQ("d", clopts_positional_at(&run.result, 0));

    run_parse(&run, 2, fourth);
    check_status(&run, CLOPTS_HELP, NULL);
    NAOS_CHECK_NULL(selected_values(&run.result));
    NAOS_CHECK_UINT_EQ(0, clopts_positional_count(&run.result));

    run_free(&run);
}

NAOS_SUITE(dispatch,
    NAOS_ADD_TEST(build_is_selected),
    NAOS_ADD_TEST(cat_is_selected_with_positionals),
    NAOS_ADD_TEST(copy_is_selected),
    NAOS_ADD_TEST(misspelled_command_is_unknown),
    NAOS_ADD_TEST(command_name_is_case_sensitive),
    NAOS_ADD_TEST(command_prefix_is_unknown),
    NAOS_ADD_TEST(single_dash_is_unknown_command),
    NAOS_ADD_TEST(option_before_command_is_unknown),
    NAOS_ADD_TEST(short_option_before_command_is_unknown),
    NAOS_ADD_TEST(unknown_long_option_with_value_shows_name_only),
    NAOS_ADD_TEST(help_with_value_takes_no_value),
    NAOS_ADD_TEST(version_with_value_takes_no_value),
    NAOS_ADD_TEST(short_option_cluster_before_command_shows_first_letter),
    NAOS_ADD_TEST(non_ascii_short_option_before_command_is_unknown),
    NAOS_ADD_TEST(double_dash_then_command_is_unexpected),
    NAOS_ADD_TEST(double_dash_alone_is_no_command),
    NAOS_ADD_TEST(no_arguments_is_no_command),
    NAOS_ADD_TEST(argc_zero_is_no_command),
    NAOS_ADD_TEST(long_help_ignores_later_tokens),
    NAOS_ADD_TEST(short_help_ignores_later_tokens),
    NAOS_ADD_TEST(help_word_alone_is_program_help),
    NAOS_ADD_TEST(help_word_with_command_is_command_help),
    NAOS_ADD_TEST(help_word_with_unknown_command),
    NAOS_ADD_TEST(help_word_with_two_arguments),
    NAOS_ADD_TEST(help_word_with_two_known_commands),
    NAOS_ADD_TEST(help_word_with_command_and_extra_argument),
    NAOS_ADD_TEST(help_word_treats_option_like_token_as_name),
    NAOS_ADD_TEST(version_ignores_later_tokens),
    NAOS_ADD_TEST(version_with_extra_argument_is_version),
    NAOS_ADD_TEST(help_before_command_is_root_help),
    NAOS_ADD_TEST(version_after_command_is_unknown_option),
    NAOS_ADD_TEST(version_with_value_after_command_is_unknown_option),
    NAOS_ADD_TEST(invalid_declaration_is_decl_error_without_reading_argv),
    NAOS_ADD_TEST(invalid_declaration_with_tokens),
    NAOS_ADD_TEST(result_is_reset_between_parses)
)
