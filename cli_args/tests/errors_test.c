#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/help.h"
#include "clopts/errors.h"
#include "test_decl.h"

/* 必須のオプションの検査、最初の誤り、ヘルプの優先、メッセージの内容、状態の区別（7.1〜7.6、8.2） */

#define ERRORS_MAX_ARGS 8
#define ERRORS_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct errors_run
{
    char storage[ERRORS_MAX_ARGS][ERRORS_MAX_LEN];
    char *argv[ERRORS_MAX_ARGS + 1];
    mnd_args_result_t result;
    clopts_errors_t errors;
    mnd_args_status_t status;
} errors_run_t;

/* tokens の argc 個を引数列として program で解析する */
static void run_parse_program(errors_run_t *run, const mnd_args_program_t *program, int argc,
                              const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    mnd_args_result_init(&run->result);
    clopts_errors_init(&run->errors);

    NAOS_REQUIRE_TRUE(argc <= ERRORS_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < ERRORS_MAX_LEN);
        strcpy(run->storage[i], tokens[i]);
        run->argv[i] = run->storage[i];
    }
    run->argv[argc] = NULL;

    run->status = mnd_args_parse(program, argc, run->argv, &run->result, &run->errors);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
}

static void run_parse(errors_run_t *run, int argc, const char *const *tokens)
{
    run_parse_program(run, &test_tool_program, argc, tokens);
}

static void run_free(errors_run_t *run)
{
    mnd_args_result_free(&run->result);
    clopts_errors_free(&run->errors);
}

/*
 * build を選択した後の使い方のエラーで、エラーがちょうど 1 件だけ追加され、
 * メッセージが期待どおりであることを確かめる（7.2、7.4、7.5）
 */
static void check_build_usage_error(const errors_run_t *run, const char *expected)
{
    const char *message;

    NAOS_CHECK_INT_EQ(MND_ARGS_USAGE_ERROR, run->status);
    NAOS_CHECK_PTR_EQ(test_tool_build, mnd_args_command(&run->result));
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&run->errors));
    message = clopts_errors_at(&run->errors, 0);
    NAOS_CHECK_STR_EQ(expected, message);
}

/* build のヘルプの要求で、エラーがないことを確かめる（8.2） */
static void check_build_help(const errors_run_t *run)
{
    NAOS_CHECK_INT_EQ(MND_ARGS_HELP, run->status);
    NAOS_CHECK_PTR_EQ(test_tool_build, mnd_args_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run->errors));
}

/* message が prefix で始まり、needle を含むことを確かめる */
static void check_message_mentions(const errors_run_t *run, const char *prefix,
                                   const char *needle)
{
    const char *message;

    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&run->errors));
    message = clopts_errors_at(&run->errors, 0);
    NAOS_CHECK_INT_EQ(0, strncmp(message, prefix, strlen(prefix)));
    NAOS_CHECK_NOT_NULL(strstr(message, needle));
}

/* 必須のオプションの欠落（7.1） */

NAOS_TEST(missing_required_option_is_reported)
{
    static const char *const tokens[] = {"tool", "build", "x"};
    errors_run_t run;

    run_parse(&run, 3, tokens);
    check_build_usage_error(&run, "tool build: missing required option '--output'");
    run_free(&run);
}

/* 位置引数の不足は必須のオプションの欠落より先に報告する（6.4、7.1） */
NAOS_TEST(missing_positional_is_checked_before_required_option)
{
    static const char *const tokens[] = {"tool", "build"};
    errors_run_t run;

    run_parse(&run, 2, tokens);
    check_build_usage_error(&run, "tool build: missing argument <SOURCE>");
    run_free(&run);
}

/* メッセージの接頭辞と問題の引数（7.2） */

NAOS_TEST(messages_start_with_command_prefix_and_name_the_argument)
{
    static const char *const unknown_long[] = {"tool", "build", "--bad", "x"};
    static const char *const unknown_short[] = {"tool", "build", "-x", "y"};
    static const char *const requires_value[] = {"tool", "build", "x", "--output"};
    static const char *const flag_value[] = {"tool", "build", "--verbose=1", "x"};
    static const char *const empty_item[] = {"tool", "build", "-k", "a,,b", "x"};
    static const char *const unexpected[] = {"tool", "build", "-o", "y", "x", "z"};
    errors_run_t run;

    run_parse(&run, 4, unknown_long);
    check_build_usage_error(&run, "tool build: unknown option '--bad'");
    check_message_mentions(&run, "tool build: ", "--bad");
    run_free(&run);

    run_parse(&run, 4, unknown_short);
    check_build_usage_error(&run, "tool build: unknown option '-x'");
    check_message_mentions(&run, "tool build: ", "-x");
    run_free(&run);

    run_parse(&run, 4, requires_value);
    check_build_usage_error(&run, "tool build: option '--output' requires a value");
    check_message_mentions(&run, "tool build: ", "--output");
    run_free(&run);

    run_parse(&run, 4, flag_value);
    check_build_usage_error(&run, "tool build: option '--verbose' does not take a value");
    check_message_mentions(&run, "tool build: ", "--verbose");
    run_free(&run);

    run_parse(&run, 5, empty_item);
    check_build_usage_error(&run, "tool build: option '-k' has an empty item in 'a,,b'");
    check_message_mentions(&run, "tool build: ", "a,,b");
    run_free(&run);

    run_parse(&run, 6, unexpected);
    check_build_usage_error(&run, "tool build: unexpected argument 'z'");
    check_message_mentions(&run, "tool build: ", "'z'");
    run_free(&run);
}

/* 最初の誤りだけを報告する（7.4） */

NAOS_TEST(only_first_error_is_reported)
{
    static const char *const tokens[] = {"tool", "build", "--bad", "--worse", "x"};
    errors_run_t run;

    run_parse(&run, 5, tokens);
    check_build_usage_error(&run, "tool build: unknown option '--bad'");
    run_free(&run);
}

NAOS_TEST(scan_error_is_reported_before_missing_required_option)
{
    static const char *const tokens[] = {"tool", "build", "x", "y"};
    errors_run_t run;

    run_parse(&run, 4, tokens);
    check_build_usage_error(&run, "tool build: unexpected argument 'y'");
    run_free(&run);
}

/* ヘルプの優先（8.2） */

NAOS_TEST(help_after_error_wins)
{
    static const char *const short_help[] = {"tool", "build", "--bad", "-h"};
    static const char *const long_help[] = {"tool", "build", "-x", "--worse", "--help"};
    errors_run_t run;

    run_parse(&run, 4, short_help);
    check_build_help(&run);
    run_free(&run);

    run_parse(&run, 5, long_help);
    check_build_help(&run);
    run_free(&run);
}

NAOS_TEST(help_wins_over_missing_required_option)
{
    static const char *const tokens[] = {"tool", "build", "x", "--help"};
    errors_run_t run;

    run_parse(&run, 4, tokens);
    check_build_help(&run);
    run_free(&run);
}

NAOS_TEST(help_wins_over_missing_positional)
{
    static const char *const tokens[] = {"tool", "copy", "-h"};
    errors_run_t run;

    run_parse(&run, 3, tokens);
    NAOS_CHECK_INT_EQ(MND_ARGS_HELP, run.status);
    NAOS_CHECK_PTR_EQ(test_tool_copy, mnd_args_command(&run.result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run.errors));
    run_free(&run);
}

NAOS_TEST(help_wins_over_extra_positional)
{
    static const char *const tokens[] = {"tool", "copy", "a", "b", "c", "--help"};
    errors_run_t run;

    run_parse(&run, 6, tokens);
    NAOS_CHECK_INT_EQ(MND_ARGS_HELP, run.status);
    NAOS_CHECK_PTR_EQ(test_tool_copy, mnd_args_command(&run.result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run.errors));
    run_free(&run);
}

/* 回復走査は "--" を越えない */
NAOS_TEST(help_after_terminator_does_not_win)
{
    static const char *const tokens[] = {"tool", "build", "--bad", "--", "-h"};
    errors_run_t run;

    run_parse(&run, 5, tokens);
    check_build_usage_error(&run, "tool build: unknown option '--bad'");
    run_free(&run);
}

/* 回復走査は完全一致だけを見る */
NAOS_TEST(recovery_ignores_inexact_help)
{
    static const char *const tokens[] = {"tool", "build", "--bad", "-vh", "--help=1", "--helpx"};
    errors_run_t run;

    run_parse(&run, 6, tokens);
    check_build_usage_error(&run, "tool build: unknown option '--bad'");
    run_free(&run);
}

/* 回復走査は誤りの原因となったトークン（消費した値を含む）の次から始める */
NAOS_TEST(recovery_starts_after_consumed_value)
{
    static const char *const tokens[] = {"tool", "build", "-k", "-h,", "x"};
    errors_run_t run;

    run_parse(&run, 5, tokens);
    check_build_usage_error(&run, "tool build: option '-k' has an empty item in '-h,'");
    run_free(&run);
}

/*
 * 誤りより前に値として消費した完全一致の -h・--help は、回復走査の対象にならない。
 * 回復走査が誤りのトークンの次から始まることを確かめる
 */
NAOS_TEST(help_consumed_before_error_does_not_win)
{
    static const char *const short_value[] = {"tool", "build", "-o", "-h", "--bad"};
    static const char *const long_value[] = {"tool", "build", "--output", "--help", "--bad"};
    errors_run_t run;

    run_parse(&run, 5, short_value);
    check_build_usage_error(&run, "tool build: unknown option '--bad'");
    run_free(&run);

    run_parse(&run, 5, long_value);
    check_build_usage_error(&run, "tool build: unknown option '--bad'");
    run_free(&run);
}

/* 誤りが末尾で起きた場合、回復走査で見るトークンは残っていない */
NAOS_TEST(error_at_end_has_nothing_to_recover)
{
    static const char *const tokens[] = {"tool", "build", "x", "-o"};
    errors_run_t run;

    run_parse(&run, 4, tokens);
    check_build_usage_error(&run, "tool build: option '-o' requires a value");
    run_free(&run);
}

/*
 * 子を持つコマンドが選択中の誤りは、後ろに --help・-h があっても報告する（8.10）。
 * 経路がルートコマンドだけで、エラーがちょうど 1 件であることを確かめる
 */
static void check_root_usage_error(const errors_run_t *run, const char *expected)
{
    const char *message;

    NAOS_CHECK_INT_EQ(MND_ARGS_USAGE_ERROR, run->status);
    NAOS_CHECK_UINT_EQ(1, mnd_args_depth(&run->result));
    NAOS_CHECK_PTR_EQ(test_tool_program.root, mnd_args_command(&run->result));
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&run->errors));
    message = clopts_errors_at(&run->errors, 0);
    NAOS_CHECK_STR_EQ(expected, message);
}

NAOS_TEST(unknown_command_before_help_is_reported)
{
    static const char *const tokens[] = {"tool", "bogus", "--help"};
    errors_run_t run;

    run_parse(&run, 3, tokens);
    check_root_usage_error(&run, "tool: unknown command 'bogus'");
    run_free(&run);
}

NAOS_TEST(unknown_option_before_command_and_help_is_reported)
{
    static const char *const tokens[] = {"tool", "--bogus", "-h"};
    errors_run_t run;

    run_parse(&run, 3, tokens);
    check_root_usage_error(&run, "tool: unknown option '--bogus'");
    run_free(&run);
}

NAOS_TEST(help_word_error_before_help_is_reported)
{
    static const char *const unknown[] = {"tool", "help", "nope", "--help"};
    static const char *const extra[] = {"tool", "help", "copy", "x", "-h"};
    errors_run_t run;

    run_parse(&run, 4, unknown);
    check_root_usage_error(&run, "tool: unknown command 'nope'");
    run_free(&run);

    run_parse(&run, 5, extra);
    check_root_usage_error(&run, "tool: unexpected argument 'x'");
    run_free(&run);
}

/* 値として消費した -h はヘルプにならない（3.2、4.1、8.2） */
NAOS_TEST(dash_h_consumed_as_value_is_not_help)
{
    static const char *const tokens[] = {"tool", "build", "-o", "-h", "x"};
    errors_run_t run;

    run_parse(&run, 5, tokens);
    NAOS_CHECK_INT_EQ(MND_ARGS_OK, run.status);
    NAOS_CHECK_PTR_EQ(test_tool_build, mnd_args_command(&run.result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run.errors));
    NAOS_CHECK_PTR_EQ(run.argv[3], mnd_args_value(&run.result, "output"));
    NAOS_CHECK_STR_EQ("-h", mnd_args_value(&run.result, "output"));
    NAOS_REQUIRE_UINT_EQ(1, mnd_args_positional_count(&run.result));
    NAOS_CHECK_STR_EQ("x", mnd_args_positional_at(&run.result, 0));
    run_free(&run);
}

/* 状態の区別（7.6） */

/* 宣言の誤りを含むプログラム（"help" という名前のサブコマンド） */
static const mnd_args_command_t errors_bad_commands[] = {
    { .name = "help", .summary = "Reserved name" }
};

static const mnd_args_command_t errors_bad_root = {
    .commands = errors_bad_commands,
    .command_count = 1
};

static const mnd_args_program_t errors_bad_program = {
    .name = "bad",
    .version = "0.0.1",
    .root = &errors_bad_root
};

NAOS_TEST(each_status_is_distinct)
{
    static const char *const ok[] = {"tool", "build", "-o", "y", "x"};
    static const char *const help[] = {"tool", "build", "x", "-h"};
    static const char *const version[] = {"tool", "--version"};
    static const char *const no_command[] = {"tool"};
    static const char *const usage_error[] = {"tool", "build", "x"};
    static const char *const decl_error[] = {"bad", "help"};
    mnd_args_status_t statuses[6];
    errors_run_t run;
    size_t i;
    size_t j;

    run_parse(&run, 5, ok);
    statuses[0] = run.status;
    NAOS_CHECK_INT_EQ(MND_ARGS_OK, run.status);
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run.errors));
    run_free(&run);

    run_parse(&run, 4, help);
    statuses[1] = run.status;
    NAOS_CHECK_INT_EQ(MND_ARGS_HELP, run.status);
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run.errors));
    run_free(&run);

    run_parse(&run, 2, version);
    statuses[2] = run.status;
    NAOS_CHECK_INT_EQ(MND_ARGS_VERSION, run.status);
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run.errors));
    run_free(&run);

    run_parse(&run, 1, no_command);
    statuses[3] = run.status;
    NAOS_CHECK_INT_EQ(MND_ARGS_NO_COMMAND, run.status);
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run.errors));
    run_free(&run);

    run_parse(&run, 3, usage_error);
    statuses[4] = run.status;
    NAOS_CHECK_INT_EQ(MND_ARGS_USAGE_ERROR, run.status);
    NAOS_CHECK_UINT_EQ(1, clopts_errors_count(&run.errors));
    run_free(&run);

    run_parse_program(&run, &errors_bad_program, 2, decl_error);
    statuses[5] = run.status;
    NAOS_CHECK_INT_EQ(MND_ARGS_DECL_ERROR, run.status);
    NAOS_CHECK_UINT_GE(clopts_errors_count(&run.errors), 1);
    run_free(&run);

    for (i = 0; i < 6; i++)
    {
        for (j = i + 1; j < 6; j++)
        {
            NAOS_CHECK_INT_NE(statuses[i], statuses[j]);
        }
    }
}

/*
 * monde-cli の典型的な使い方: 解析が使い方のエラーを返したら、結果に残った
 * 経路で使い方の案内を生成する。経路がルートコマンドだけなら全体のヘルプを出す（7.3、11.1〜11.3）
 */
NAOS_TEST(usage_error_then_hint_for_selected_command)
{
    static const char *const usage_error[] = {"tool", "build", "--bad", "x"};
    static const char *const no_command[] = {"tool"};
    static const char global_help_head[] = "Usage: tool <command> [<options>] [<arguments>]\n";
    errors_run_t run;
    char *text = NULL;

    run_parse(&run, 4, usage_error);
    check_build_usage_error(&run, "tool build: unknown option '--bad'");
    text = mnd_args_format_usage_hint(&run.result);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_CHECK_STR_EQ("Run 'tool build --help' for usage.\n", text);
    mnd_args_text_free(text);
    run_free(&run);

    run_parse(&run, 1, no_command);
    NAOS_CHECK_INT_EQ(MND_ARGS_NO_COMMAND, run.status);
    NAOS_CHECK_UINT_EQ(1, mnd_args_depth(&run.result));
    NAOS_CHECK_PTR_EQ(test_tool_program.root, mnd_args_command(&run.result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run.errors));
    text = mnd_args_format_help(&run.result);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_CHECK_INT_EQ(0, strncmp(text, global_help_head, strlen(global_help_head)));
    mnd_args_text_free(text);
    run_free(&run);
}

NAOS_SUITE(errors,
    NAOS_ADD_TEST(missing_required_option_is_reported),
    NAOS_ADD_TEST(missing_positional_is_checked_before_required_option),
    NAOS_ADD_TEST(messages_start_with_command_prefix_and_name_the_argument),
    NAOS_ADD_TEST(only_first_error_is_reported),
    NAOS_ADD_TEST(scan_error_is_reported_before_missing_required_option),
    NAOS_ADD_TEST(help_after_error_wins),
    NAOS_ADD_TEST(help_wins_over_missing_required_option),
    NAOS_ADD_TEST(help_wins_over_missing_positional),
    NAOS_ADD_TEST(help_wins_over_extra_positional),
    NAOS_ADD_TEST(help_after_terminator_does_not_win),
    NAOS_ADD_TEST(recovery_ignores_inexact_help),
    NAOS_ADD_TEST(recovery_starts_after_consumed_value),
    NAOS_ADD_TEST(help_consumed_before_error_does_not_win),
    NAOS_ADD_TEST(error_at_end_has_nothing_to_recover),
    NAOS_ADD_TEST(unknown_command_before_help_is_reported),
    NAOS_ADD_TEST(unknown_option_before_command_and_help_is_reported),
    NAOS_ADD_TEST(help_word_error_before_help_is_reported),
    NAOS_ADD_TEST(dash_h_consumed_as_value_is_not_help),
    NAOS_ADD_TEST(each_status_is_distinct),
    NAOS_ADD_TEST(usage_error_then_hint_for_selected_command)
)
