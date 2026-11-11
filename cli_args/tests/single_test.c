#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/help.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "test_decl.h"

/* サブコマンドのないプログラム（ルートコマンドが末端コマンド）の解析（2.6、6.x、8.x） */

#define SINGLE_MAX_ARGS 8
#define SINGLE_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct single_run
{
    char storage[SINGLE_MAX_ARGS][SINGLE_MAX_LEN];
    char *argv[SINGLE_MAX_ARGS + 1];
    mnd_args_result_t result;
    mnd_diag_list_t diags;
    mnd_args_status_t status;
} single_run_t;

/* tokens の argc 個を引数列として test_single_program で解析する */
static void run_parse(single_run_t *run, int argc, const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    mnd_args_result_init(&run->result);
    mnd_diag_list_init(&run->diags);

    NAOS_REQUIRE_TRUE(argc <= SINGLE_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < SINGLE_MAX_LEN);
        strcpy(run->storage[i], tokens[i]);
        run->argv[i] = run->storage[i];
    }
    run->argv[argc] = NULL;

    run->status = mnd_args_parse(&test_single_program, argc, run->argv, &run->result, &run->diags);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
}

static void run_free(single_run_t *run)
{
    mnd_args_result_free(&run->result);
    mnd_diag_list_free(&run->diags);
}

/* 診断なしで status になり、経路がルートコマンドだけの 1 段であることを確かめる（2.8） */
static void check_status(const single_run_t *run, mnd_args_status_t status)
{
    NAOS_CHECK_INT_EQ(status, run->status);
    NAOS_CHECK_UINT_EQ(1, mnd_args_depth(&run->result));
    NAOS_CHECK_PTR_EQ(test_single_program.root, mnd_args_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&run->diags));
}

/*
 * ルートコマンド（末端コマンド）のヘルプの期待する文面。設計の書式から手で導いたもの。
 * Options: 左列の最大幅は "-k, --keys <KEY>[,<KEY>...]" の 27。ルートコマンドなので --version の行がある
 */
static const char expected_single_help[] =
    "Usage: single [<options>] [<FILE>...]\n"
    "\n"
    "A sample tool without subcommands.\n"
    "\n"
    "Arguments:\n"
    "  <FILE>  Files to process\n"
    "\n"
    "Options:\n"
    "  -o, --output <FILE>          Write the result to FILE\n"
    "  -v, --verbose                Print more details\n"
    "  -k, --keys <KEY>[,<KEY>...]  Keys to include\n"
    "  -h, --help                   Show this help\n"
    "      --version                Show version information\n";

/* 生成した文面を期待する文面とバイト単位で比べる */
static void check_text(const char *expected, const mnd_buf_t *buf)
{
    NAOS_CHECK_STR_EQ(expected, mnd_buf_cstr(buf));
    NAOS_CHECK_MEM_EQ(expected, strlen(expected), mnd_buf_cstr(buf), buf->len);
}

/* 使い方のエラーで、期待するメッセージの診断がちょうど 1 件だけ追加されたことを確かめる（7.2、7.4） */
static void check_usage_error(const single_run_t *run, const char *expected)
{
    const mnd_diag_t *d;

    NAOS_CHECK_INT_EQ(MND_ARGS_USAGE_ERROR, run->status);
    NAOS_CHECK_UINT_EQ(1, mnd_args_depth(&run->result));
    NAOS_CHECK_PTR_EQ(test_single_program.root, mnd_args_command(&run->result));
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&run->diags));
    d = mnd_diag_list_at(&run->diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_CHECK_NULL(d->path);
    NAOS_CHECK_UINT_EQ(0, d->line);
    NAOS_CHECK_STR_EQ(expected, d->message);
}

/* 引数なしの実行は通常の解析（2.6、6.6） */

NAOS_TEST(no_arguments_is_ok_without_positionals)
{
    static const char *const tokens[] = {"single"};
    single_run_t run;

    run_parse(&run, 1, tokens);
    check_status(&run, MND_ARGS_OK);
    NAOS_CHECK_UINT_EQ(0, mnd_args_positional_count(&run.result));
    NAOS_CHECK_FALSE(mnd_args_flag(&run.result, "verbose"));
    NAOS_CHECK_NULL(mnd_args_value(&run.result, "output"));
    NAOS_CHECK_UINT_EQ(0, mnd_args_list_count(&run.result, "keys"));
    run_free(&run);
}

NAOS_TEST(argc_zero_is_ok)
{
    char *argv[] = {NULL};
    mnd_args_result_t result = MND_ARGS_RESULT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_INT_EQ(MND_ARGS_OK, mnd_args_parse(&test_single_program, 0, argv, &result, &diags));
    NAOS_CHECK_UINT_EQ(1, mnd_args_depth(&result));
    NAOS_CHECK_UINT_EQ(0, mnd_args_positional_count(&result));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_args_result_free(&result);
    mnd_diag_list_free(&diags);
}

/* どの引数もサブコマンド名として解釈しない（2.6、8.7） */

NAOS_TEST(help_word_is_positional)
{
    static const char *const tokens[] = {"single", "help"};
    single_run_t run;

    run_parse(&run, 2, tokens);
    check_status(&run, MND_ARGS_OK);
    NAOS_REQUIRE_UINT_EQ(1, mnd_args_positional_count(&run.result));
    NAOS_CHECK_PTR_EQ(run.argv[1], mnd_args_positional_at(&run.result, 0));
    run_free(&run);
}

NAOS_TEST(command_like_word_is_positional)
{
    static const char *const tokens[] = {"single", "build", "help", "x"};
    single_run_t run;

    run_parse(&run, 4, tokens);
    check_status(&run, MND_ARGS_OK);
    NAOS_REQUIRE_UINT_EQ(3, mnd_args_positional_count(&run.result));
    NAOS_CHECK_STR_EQ("build", mnd_args_positional_at(&run.result, 0));
    NAOS_CHECK_STR_EQ("help", mnd_args_positional_at(&run.result, 1));
    NAOS_CHECK_STR_EQ("x", mnd_args_positional_at(&run.result, 2));
    run_free(&run);
}

/* オプションと位置引数の混在（3.x、4.x、6.2） */

NAOS_TEST(option_position_does_not_change_result)
{
    static const char *const between[] = {"single", "a", "-v", "b"};
    static const char *const before[] = {"single", "-v", "a", "b"};
    single_run_t first;
    single_run_t second;
    size_t i;

    run_parse(&first, 4, between);
    run_parse(&second, 4, before);
    check_status(&first, MND_ARGS_OK);
    check_status(&second, MND_ARGS_OK);
    NAOS_CHECK_TRUE(mnd_args_flag(&first.result, "verbose"));
    NAOS_CHECK_TRUE(mnd_args_flag(&second.result, "verbose"));
    NAOS_REQUIRE_UINT_EQ(2, mnd_args_positional_count(&first.result));
    NAOS_REQUIRE_UINT_EQ(2, mnd_args_positional_count(&second.result));
    for (i = 0; i < 2; i++)
    {
        NAOS_CHECK_STR_EQ(mnd_args_positional_at(&second.result, i),
                          mnd_args_positional_at(&first.result, i));
    }
    NAOS_CHECK_STR_EQ("a", mnd_args_positional_at(&first.result, 0));
    NAOS_CHECK_STR_EQ("b", mnd_args_positional_at(&first.result, 1));
    run_free(&first);
    run_free(&second);
}

NAOS_TEST(values_and_lists_are_recorded)
{
    static const char *const tokens[] = {"single", "-o", "out", "--keys=x,y", "-kz", "f"};
    single_run_t run;

    run_parse(&run, 6, tokens);
    check_status(&run, MND_ARGS_OK);
    NAOS_CHECK_STR_EQ("out", mnd_args_value(&run.result, "output"));
    NAOS_REQUIRE_UINT_EQ(3, mnd_args_list_count(&run.result, "keys"));
    NAOS_CHECK_STR_EQ("x", mnd_args_list_at(&run.result, "keys", 0));
    NAOS_CHECK_STR_EQ("y", mnd_args_list_at(&run.result, "keys", 1));
    NAOS_CHECK_STR_EQ("z", mnd_args_list_at(&run.result, "keys", 2));
    NAOS_REQUIRE_UINT_EQ(1, mnd_args_positional_count(&run.result));
    NAOS_CHECK_STR_EQ("f", mnd_args_positional_at(&run.result, 0));
    run_free(&run);
}

NAOS_TEST(double_dash_makes_rest_positional)
{
    static const char *const tokens[] = {"single", "--", "-v", "--help", "help"};
    single_run_t run;

    run_parse(&run, 5, tokens);
    check_status(&run, MND_ARGS_OK);
    NAOS_CHECK_FALSE(mnd_args_flag(&run.result, "verbose"));
    NAOS_REQUIRE_UINT_EQ(3, mnd_args_positional_count(&run.result));
    NAOS_CHECK_STR_EQ("-v", mnd_args_positional_at(&run.result, 0));
    NAOS_CHECK_STR_EQ("--help", mnd_args_positional_at(&run.result, 1));
    NAOS_CHECK_STR_EQ("help", mnd_args_positional_at(&run.result, 2));
    run_free(&run);
}

/* ヘルプとバージョン（8.1、8.2、8.8） */

NAOS_TEST(long_help_is_root_help)
{
    static const char *const tokens[] = {"single", "a", "--help"};
    single_run_t run;

    run_parse(&run, 3, tokens);
    check_status(&run, MND_ARGS_HELP);
    run_free(&run);
}

NAOS_TEST(short_help_in_cluster_is_root_help)
{
    static const char *const tokens[] = {"single", "-vh"};
    single_run_t run;

    run_parse(&run, 2, tokens);
    check_status(&run, MND_ARGS_HELP);
    run_free(&run);
}

/* 末端コマンドのルートでも回復走査が働き、ルートコマンドのヘルプになる（8.3） */
NAOS_TEST(help_after_error_is_root_help)
{
    static const char *const long_help[] = {"single", "--bad", "--help"};
    static const char *const short_help[] = {"single", "a", "-x", "b", "-h"};
    single_run_t run;

    run_parse(&run, 3, long_help);
    check_status(&run, MND_ARGS_HELP);
    run_free(&run);

    run_parse(&run, 5, short_help);
    check_status(&run, MND_ARGS_HELP);
    run_free(&run);
}

/* 回復走査は "--" を越えない（8.3） */
NAOS_TEST(help_after_terminator_does_not_recover)
{
    static const char *const tokens[] = {"single", "--bad", "--", "--help"};
    single_run_t run;

    run_parse(&run, 4, tokens);
    check_usage_error(&run, "single: unknown option '--bad'");
    run_free(&run);
}

NAOS_TEST(version_is_version)
{
    static const char *const tokens[] = {"single", "--version"};
    single_run_t run;

    run_parse(&run, 2, tokens);
    check_status(&run, MND_ARGS_VERSION);
    run_free(&run);
}

NAOS_TEST(version_with_extra_argument_is_version)
{
    static const char *const tokens[] = {"single", "--version", "x"};
    single_run_t run;

    run_parse(&run, 3, tokens);
    check_status(&run, MND_ARGS_VERSION);
    run_free(&run);
}

NAOS_TEST(version_after_positional_is_version)
{
    static const char *const tokens[] = {"single", "a", "-v", "--version"};
    single_run_t run;

    run_parse(&run, 4, tokens);
    check_status(&run, MND_ARGS_VERSION);
    run_free(&run);
}

NAOS_TEST(version_consumed_as_value_is_not_version)
{
    static const char *const tokens[] = {"single", "-o", "--version"};
    single_run_t run;

    run_parse(&run, 3, tokens);
    check_status(&run, MND_ARGS_OK);
    NAOS_CHECK_STR_EQ("--version", mnd_args_value(&run.result, "output"));
    run_free(&run);
}

/* 使い方のエラーの接頭辞はルートコマンドの経路（3.5、3.6、4.5、7.2） */

NAOS_TEST(unknown_option_has_root_prefix)
{
    static const char *const tokens[] = {"single", "--bad"};
    single_run_t run;

    run_parse(&run, 2, tokens);
    check_usage_error(&run, "single: unknown option '--bad'");
    run_free(&run);
}

NAOS_TEST(unknown_short_option_has_root_prefix)
{
    static const char *const tokens[] = {"single", "a", "-x"};
    single_run_t run;

    run_parse(&run, 3, tokens);
    check_usage_error(&run, "single: unknown option '-x'");
    run_free(&run);
}

NAOS_TEST(version_with_value_takes_no_value)
{
    static const char *const tokens[] = {"single", "--version=1"};
    single_run_t run;

    run_parse(&run, 2, tokens);
    check_usage_error(&run, "single: option '--version' does not take a value");
    run_free(&run);
}

NAOS_TEST(flag_with_value_takes_no_value)
{
    static const char *const tokens[] = {"single", "--verbose=yes"};
    single_run_t run;

    run_parse(&run, 2, tokens);
    check_usage_error(&run, "single: option '--verbose' does not take a value");
    run_free(&run);
}

NAOS_TEST(missing_value_has_root_prefix)
{
    static const char *const tokens[] = {"single", "a", "--output"};
    single_run_t run;

    run_parse(&run, 3, tokens);
    check_usage_error(&run, "single: option '--output' requires a value");
    run_free(&run);
}

/* ヘルプと案内の文面（7.3、9.1、9.3、9.4、9.9） */

NAOS_TEST(help_text_matches_expected_text)
{
    static const char *const tokens[] = {"single", "--help"};
    single_run_t run;
    mnd_buf_t buf = MND_BUF_INIT;

    run_parse(&run, 2, tokens);
    check_status(&run, MND_ARGS_HELP);
    NAOS_REQUIRE_TRUE(mnd_args_format_help(&run.result, &buf));
    check_text(expected_single_help, &buf);

    mnd_buf_free(&buf);
    run_free(&run);
}

NAOS_TEST(help_text_is_identical_when_generated_twice)
{
    static const char *const tokens[] = {"single", "--help"};
    single_run_t run;
    mnd_buf_t first = MND_BUF_INIT;
    mnd_buf_t second = MND_BUF_INIT;

    run_parse(&run, 2, tokens);
    NAOS_REQUIRE_TRUE(mnd_args_format_help(&run.result, &first));
    NAOS_REQUIRE_TRUE(mnd_args_format_help(&run.result, &second));
    NAOS_CHECK_UINT_GT(first.len, 0);
    NAOS_CHECK_MEM_EQ(first.data, first.len, second.data, second.len);

    mnd_buf_free(&first);
    mnd_buf_free(&second);
    run_free(&run);
}

NAOS_TEST(usage_hint_has_root_path)
{
    static const char *const tokens[] = {"single", "--bad"};
    single_run_t run;
    mnd_buf_t buf = MND_BUF_INIT;

    run_parse(&run, 2, tokens);
    check_usage_error(&run, "single: unknown option '--bad'");
    NAOS_REQUIRE_TRUE(mnd_args_format_usage_hint(&run.result, &buf));
    check_text("Run 'single --help' for usage.\n", &buf);

    mnd_buf_free(&buf);
    run_free(&run);
}

NAOS_SUITE(single,
    NAOS_ADD_TEST(no_arguments_is_ok_without_positionals),
    NAOS_ADD_TEST(argc_zero_is_ok),
    NAOS_ADD_TEST(help_word_is_positional),
    NAOS_ADD_TEST(command_like_word_is_positional),
    NAOS_ADD_TEST(option_position_does_not_change_result),
    NAOS_ADD_TEST(values_and_lists_are_recorded),
    NAOS_ADD_TEST(double_dash_makes_rest_positional),
    NAOS_ADD_TEST(long_help_is_root_help),
    NAOS_ADD_TEST(short_help_in_cluster_is_root_help),
    NAOS_ADD_TEST(help_after_error_is_root_help),
    NAOS_ADD_TEST(help_after_terminator_does_not_recover),
    NAOS_ADD_TEST(version_is_version),
    NAOS_ADD_TEST(version_with_extra_argument_is_version),
    NAOS_ADD_TEST(version_after_positional_is_version),
    NAOS_ADD_TEST(version_consumed_as_value_is_not_version),
    NAOS_ADD_TEST(unknown_option_has_root_prefix),
    NAOS_ADD_TEST(unknown_short_option_has_root_prefix),
    NAOS_ADD_TEST(version_with_value_takes_no_value),
    NAOS_ADD_TEST(flag_with_value_takes_no_value),
    NAOS_ADD_TEST(missing_value_has_root_prefix),
    NAOS_ADD_TEST(help_text_matches_expected_text),
    NAOS_ADD_TEST(help_text_is_identical_when_generated_twice),
    NAOS_ADD_TEST(usage_hint_has_root_path)
)
