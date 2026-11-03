#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

#include "cli_args/args.h"
#include "foundation/diag.h"
#include "test_decl.h"

/* サブコマンドより前の振り分け（2.x、8.x）と、解析の入口（1.6、7.5、7.6） */

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
    mnd_args_result_t result;
    mnd_diag_list_t diags;
    mnd_args_status_t status;
} dispatch_run_t;

static void run_init(dispatch_run_t *run)
{
    memset(run, 0, sizeof *run);
    mnd_args_result_init(&run->result);
    mnd_diag_list_init(&run->diags);
}

/* tokens の argc 個を引数列として解析する。run の結果と診断は前回の内容に上書き・追加される */
static void run_parse_with(dispatch_run_t *run, const mnd_args_program_t *program, int argc,
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

    run->status = mnd_args_parse(program, argc, run->argv, &run->result, &run->diags);
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
    mnd_args_result_free(&run->result);
    mnd_diag_list_free(&run->diags);
}

/* 使い方のエラーで、期待するメッセージの診断がちょうど 1 件だけ追加されたことを確かめる（7.4、7.5） */
static void check_usage_error(const dispatch_run_t *run, const char *expected)
{
    const mnd_diag_t *d;

    NAOS_CHECK_INT_EQ(MND_ARGS_USAGE_ERROR, run->status);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&run->diags));
    d = mnd_diag_list_at(&run->diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_CHECK_NULL(d->path);
    NAOS_CHECK_UINT_EQ(0, d->line);
    NAOS_CHECK_STR_EQ(expected, d->message);
}

/* 診断なしで status になり、command が選択されていることを確かめる */
static void check_status(const dispatch_run_t *run, mnd_args_status_t status,
                         const mnd_args_command_t *command)
{
    NAOS_CHECK_INT_EQ(status, run->status);
    NAOS_CHECK_PTR_EQ(command, run->result.command);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&run->diags));
}

/* 宣言済みのサブコマンドの選択（2.1） */

NAOS_TEST(build_is_selected)
{
    static const char *const tokens[] = {"tool", "build", "x", "-o", "y"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 5, tokens);
    check_status(&run, MND_ARGS_OK, test_tool_build);
    run_free(&run);
}

NAOS_TEST(cat_is_selected_with_positionals)
{
    static const char *const tokens[] = {"tool", "cat", "a", "b"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_status(&run, MND_ARGS_OK, test_tool_cat);
    NAOS_REQUIRE_UINT_EQ(2, mnd_args_positional_count(&run.result));
    NAOS_CHECK_PTR_EQ(run.argv[2], mnd_args_positional_at(&run.result, 0));
    NAOS_CHECK_PTR_EQ(run.argv[3], mnd_args_positional_at(&run.result, 1));
    run_free(&run);
}

NAOS_TEST(copy_is_selected)
{
    static const char *const tokens[] = {"tool", "copy", "a", "b"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_status(&run, MND_ARGS_OK, test_tool_copy);
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
    NAOS_CHECK_NULL(run.result.command);
    run_free(&run);
}

NAOS_TEST(command_name_is_case_sensitive)
{
    static const char *const tokens[] = {"tool", "Build"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_usage_error(&run, "tool: unknown command 'Build'");
    NAOS_CHECK_NULL(run.result.command);
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
    NAOS_CHECK_NULL(run.result.command);
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
    NAOS_CHECK_NULL(run.result.command);
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

NAOS_TEST(double_dash_before_command_is_unknown_option)
{
    static const char *const tokens[] = {"tool", "--", "build"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool: unknown option '--'");
    run_free(&run);
}

/* 引数なし（8.6） */

NAOS_TEST(no_arguments_is_no_command)
{
    static const char *const tokens[] = {"tool"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 1, tokens);
    check_status(&run, MND_ARGS_NO_COMMAND, NULL);
    run_free(&run);
}

NAOS_TEST(argc_zero_is_no_command)
{
    char *argv[] = {NULL};
    mnd_args_result_t result = MND_ARGS_RESULT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_INT_EQ(MND_ARGS_NO_COMMAND, mnd_args_parse(&test_tool_program, 0, argv, &result, &diags));
    NAOS_CHECK_INT_EQ(MND_ARGS_NO_COMMAND, result.status);
    NAOS_CHECK_NULL(result.command);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_args_result_free(&result);
    mnd_diag_list_free(&diags);
}

/* ヘルプとバージョン（8.1、8.3、8.4、8.5） */

NAOS_TEST(long_help_ignores_later_tokens)
{
    static const char *const tokens[] = {"tool", "--help", "bulid", "--bad"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_status(&run, MND_ARGS_HELP, NULL);
    run_free(&run);
}

NAOS_TEST(short_help_ignores_later_tokens)
{
    static const char *const tokens[] = {"tool", "-h", "build", "--bad"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_status(&run, MND_ARGS_HELP, NULL);
    run_free(&run);
}

NAOS_TEST(help_word_alone_is_program_help)
{
    static const char *const tokens[] = {"tool", "help"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 2, tokens);
    check_status(&run, MND_ARGS_HELP, NULL);
    run_free(&run);
}

NAOS_TEST(help_word_with_command_is_command_help)
{
    static const char *const tokens[] = {"tool", "help", "copy"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_status(&run, MND_ARGS_HELP, test_tool_copy);
    NAOS_CHECK_NULL(run.result.values);
    NAOS_CHECK_UINT_EQ(0, mnd_args_positional_count(&run.result));
    run_free(&run);
}

NAOS_TEST(help_word_with_unknown_command)
{
    static const char *const tokens[] = {"tool", "help", "nope"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_usage_error(&run, "tool: unknown command 'nope'");
    NAOS_CHECK_NULL(run.result.command);
    run_free(&run);
}

NAOS_TEST(help_word_with_two_arguments)
{
    static const char *const tokens[] = {"tool", "help", "a", "b"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_usage_error(&run, "tool: too many arguments for 'help'");
    NAOS_CHECK_NULL(run.result.command);
    run_free(&run);
}

NAOS_TEST(help_word_with_two_known_commands)
{
    static const char *const tokens[] = {"tool", "help", "copy", "build"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 4, tokens);
    check_usage_error(&run, "tool: too many arguments for 'help'");
    run_free(&run);
}

NAOS_TEST(version_ignores_later_tokens)
{
    static const char *const tokens[] = {"tool", "--version", "bulid"};
    dispatch_run_t run;

    run_init(&run);
    run_parse(&run, 3, tokens);
    check_status(&run, MND_ARGS_VERSION, NULL);
    run_free(&run);
}

/* 宣言の誤り（1.6）: 引数列を読まずに返す */

static const mnd_args_command_t dispatch_bad_commands[] = {
    {"help", "Show help", NULL, NULL, 0, NULL, 0, 0, 0}
};

static const mnd_args_program_t dispatch_bad_program = {
    "bad", NULL, "0.1", dispatch_bad_commands, 1
};

NAOS_TEST(invalid_declaration_is_decl_error_without_reading_argv)
{
    /* 要素を読めばクラッシュする引数列。宣言の誤りでは読まれないことを確かめる */
    char *argv[] = {NULL, NULL, NULL, NULL};
    mnd_args_result_t result = MND_ARGS_RESULT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    size_t i;

    NAOS_CHECK_INT_EQ(MND_ARGS_DECL_ERROR,
                      mnd_args_parse(&dispatch_bad_program, 3, argv, &result, &diags));
    NAOS_CHECK_INT_EQ(MND_ARGS_DECL_ERROR, result.status);
    NAOS_CHECK_NULL(result.command);
    NAOS_REQUIRE_UINT_GT(mnd_diag_list_count(&diags), 0);
    for (i = 0; i < mnd_diag_list_count(&diags); i++)
    {
        const mnd_diag_t *d = mnd_diag_list_at(&diags, i);

        NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
        NAOS_CHECK_TRUE(strncmp(d->message, "invalid argument declaration: ",
                                strlen("invalid argument declaration: ")) == 0);
    }
    mnd_args_result_free(&result);
    mnd_diag_list_free(&diags);
}

NAOS_TEST(invalid_declaration_with_tokens)
{
    static const char *const tokens[] = {"bad", "--help"};
    dispatch_run_t run;

    run_init(&run);
    run_parse_with(&run, &dispatch_bad_program, 2, tokens);
    NAOS_CHECK_INT_EQ(MND_ARGS_DECL_ERROR, run.status);
    NAOS_CHECK_UINT_GT(mnd_diag_list_count(&run.diags), 0);
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
    NAOS_CHECK_PTR_EQ(test_tool_build, run.result.command);
    NAOS_CHECK_NOT_NULL(run.result.values);
    mnd_diag_list_free(&run.diags);

    run_parse(&run, 5, second);
    check_status(&run, MND_ARGS_OK, test_tool_cat);
    NAOS_CHECK_NULL(run.result.values);
    NAOS_CHECK_UINT_EQ(3, mnd_args_positional_count(&run.result));

    /* 前回の位置引数は残らない */
    run_parse(&run, 3, third);
    check_status(&run, MND_ARGS_OK, test_tool_cat);
    NAOS_REQUIRE_UINT_EQ(1, mnd_args_positional_count(&run.result));
    NAOS_CHECK_STR_EQ("d", mnd_args_positional_at(&run.result, 0));

    run_parse(&run, 2, fourth);
    check_status(&run, MND_ARGS_HELP, NULL);
    NAOS_CHECK_NULL(run.result.values);
    NAOS_CHECK_UINT_EQ(0, mnd_args_positional_count(&run.result));

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
    NAOS_ADD_TEST(double_dash_before_command_is_unknown_option),
    NAOS_ADD_TEST(no_arguments_is_no_command),
    NAOS_ADD_TEST(argc_zero_is_no_command),
    NAOS_ADD_TEST(long_help_ignores_later_tokens),
    NAOS_ADD_TEST(short_help_ignores_later_tokens),
    NAOS_ADD_TEST(help_word_alone_is_program_help),
    NAOS_ADD_TEST(help_word_with_command_is_command_help),
    NAOS_ADD_TEST(help_word_with_unknown_command),
    NAOS_ADD_TEST(help_word_with_two_arguments),
    NAOS_ADD_TEST(help_word_with_two_known_commands),
    NAOS_ADD_TEST(version_ignores_later_tokens),
    NAOS_ADD_TEST(invalid_declaration_is_decl_error_without_reading_argv),
    NAOS_ADD_TEST(invalid_declaration_with_tokens),
    NAOS_ADD_TEST(result_is_reset_between_parses)
)
