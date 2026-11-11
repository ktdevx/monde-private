#include <naos/naos.h>

#include <string.h>

#include "clopts/help.h"
#include "clopts_internal.h"
#include "test_decl.h"

/*
 * 期待する文面は設計の書式から手で導いたもの。
 * 列の揃えは段落ごとに左列の最大幅を求め、説明を 2 + 最大幅 + 2 バイト目から始める
 */

/* Commands: 左列の最大幅は "build" の 5。Options: 最大幅は "    --version" の 13 */
static const char expected_global_help[] =
    "Usage: tool <command> [<options>] [<arguments>]\n"
    "\n"
    "A sample tool for testing.\n"
    "\n"
    "Commands:\n"
    "  build  Build an output from a source\n"
    "  copy   Copy a file\n"
    "  cat    Print files\n"
    "  help   Show help for a command\n"
    "\n"
    "Options:\n"
    "  -h, --help     Show this help\n"
    "      --version  Show version information\n"
    "\n"
    "Run 'tool <command> --help' for help on a command.\n";

/* Options: 左列の最大幅は "-k, --keys <KEY>[,<KEY>...]" の 27。説明は 31 バイト目から */
static const char expected_build_help[] =
    "Usage: tool build [<options>] <SOURCE>\n"
    "\n"
    "Build an output file from SOURCE.\n"
    "\n"
    "Arguments:\n"
    "  <SOURCE>  Source file to build\n"
    "\n"
    "Options:\n"
    "  -o, --output <FILE>          Write the result to FILE (required)\n"
    "  -k, --keys <KEY>[,<KEY>...]  Keys to include (default: a,b)\n"
    "      --level <N>              Processing level (default: 1)\n"
    "  -v, --verbose                Print more details\n"
    "  -q, --quiet                  Print only errors\n"
    "  -h, --help                   Show this help\n";

/* description が NULL なので summary を使う。Arguments: 最大幅は "<DEST>" の 6 */
static const char expected_copy_help[] =
    "Usage: tool copy [<options>] <SRC> <DEST>\n"
    "\n"
    "Copy a file\n"
    "\n"
    "Arguments:\n"
    "  <SRC>   File to copy\n"
    "  <DEST>  Destination path\n"
    "\n"
    "Options:\n"
    "  -h, --help  Show this help\n";

/* 0 個以上・上限なしなので、最後の（唯一の）名前は [<FILE>...] */
static const char expected_cat_help[] =
    "Usage: tool cat [<options>] [<FILE>...]\n"
    "\n"
    "Print files\n"
    "\n"
    "Arguments:\n"
    "  <FILE>  Files to print\n"
    "\n"
    "Options:\n"
    "  -h, --help  Show this help\n";

/*
 * test_tool_program の経路を result に作る。command が NULL ならルートコマンドだけ、
 * そうでなければルートコマンドと command の 2 段
 */
static void make_path(mnd_args_result_t *result, const mnd_args_command_t *command)
{
    mnd_args_result_init(result);
    result->program = &test_tool_program;
    NAOS_REQUIRE_TRUE(mnd_args_result_push(result, test_tool_program.root));
    if (command != NULL)
    {
        NAOS_REQUIRE_TRUE(mnd_args_result_push(result, command));
    }
}

static void check_help(const mnd_args_command_t *command, const char *expected)
{
    mnd_args_result_t result;
    char *text = NULL;

    make_path(&result, command);
    text = mnd_args_format_help(&result);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_CHECK_STR_EQ(expected, text);
    NAOS_CHECK_MEM_EQ(expected, strlen(expected), text, strlen(text));

    mnd_args_text_free(text);
    mnd_args_result_free(&result);
}

NAOS_TEST(global_help_matches_expected_text)
{
    check_help(NULL, expected_global_help);
}

NAOS_TEST(build_help_shows_options_annotations_and_list_values)
{
    check_help(test_tool_build, expected_build_help);
}

NAOS_TEST(copy_help_uses_summary_and_required_positionals)
{
    check_help(test_tool_copy, expected_copy_help);
}

NAOS_TEST(cat_help_shows_optional_unlimited_positional)
{
    check_help(test_tool_cat, expected_cat_help);
}

NAOS_TEST(version_is_program_and_version_line)
{
    char *text = NULL;

    text = mnd_args_format_version(&test_tool_program);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_CHECK_STR_EQ("tool 1.2.3\n", text);
    NAOS_CHECK_UINT_EQ(11, strlen(text));

    mnd_args_text_free(text);

    /* 空の値を渡しても安全 */
    mnd_args_text_free(NULL);
}

NAOS_TEST(usage_hint_without_command)
{
    mnd_args_result_t result;
    char *text = NULL;

    make_path(&result, NULL);
    text = mnd_args_format_usage_hint(&result);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_CHECK_STR_EQ("Run 'tool --help' for usage.\n", text);

    mnd_args_text_free(text);
    mnd_args_result_free(&result);
}

NAOS_TEST(usage_hint_with_command)
{
    mnd_args_result_t result;
    char *text = NULL;

    make_path(&result, test_tool_build);
    text = mnd_args_format_usage_hint(&result);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_CHECK_STR_EQ("Run 'tool build --help' for usage.\n", text);

    mnd_args_text_free(text);
    mnd_args_result_free(&result);
}

NAOS_TEST(generating_twice_gives_identical_text)
{
    const mnd_args_command_t *root = test_tool_program.root;
    mnd_args_result_t result;
    char *first = NULL;
    char *second = NULL;
    size_t i;

    for (i = 0; i <= root->command_count; i++)
    {
        const mnd_args_command_t *command = NULL;

        if (i < root->command_count)
        {
            command = &root->commands[i];
        }
        make_path(&result, command);
        first = mnd_args_format_help(&result);
        second = mnd_args_format_help(&result);
        mnd_args_result_free(&result);
        NAOS_REQUIRE_NOT_NULL(first);
        NAOS_REQUIRE_NOT_NULL(second);
        NAOS_CHECK_UINT_GT(strlen(first), 0);
        NAOS_CHECK_UINT_EQ(strlen(first), strlen(second));
        NAOS_CHECK_STR_EQ(first, second);
        mnd_args_text_free(first);
        mnd_args_text_free(second);
        first = NULL;
        second = NULL;
    }
}

/*
 * 続けて生成した文字列は、それぞれ別に確保された独立したものである。
 * 一方を書き換えても解放しても、他方の内容は変わらない
 */
NAOS_TEST(consecutive_texts_are_independent)
{
    mnd_args_result_t copy;
    mnd_args_result_t cat;
    char *version;
    char *hint;
    char *help;

    make_path(&copy, test_tool_copy);
    make_path(&cat, test_tool_cat);
    version = mnd_args_format_version(&test_tool_program);
    hint = mnd_args_format_usage_hint(&copy);
    help = mnd_args_format_help(&cat);
    mnd_args_result_free(&copy);
    mnd_args_result_free(&cat);
    NAOS_REQUIRE_NOT_NULL(version);
    NAOS_REQUIRE_NOT_NULL(hint);
    NAOS_REQUIRE_NOT_NULL(help);
    NAOS_CHECK_TRUE(version != hint);
    NAOS_CHECK_TRUE(version != help);
    NAOS_CHECK_TRUE(hint != help);

    /* それぞれが自分の文面だけを持つ（前の文字列の後ろに追記されていない） */
    NAOS_CHECK_STR_EQ("tool 1.2.3\n", version);
    NAOS_CHECK_STR_EQ("Run 'tool copy --help' for usage.\n", hint);
    NAOS_CHECK_STR_EQ(expected_cat_help, help);

    /* 一方を書き換えても、他方は変わらない */
    version[0] = 'X';
    NAOS_CHECK_STR_EQ("Run 'tool copy --help' for usage.\n", hint);
    NAOS_CHECK_STR_EQ(expected_cat_help, help);

    /* 一方を解放しても、他方は読める */
    mnd_args_text_free(version);
    mnd_args_text_free(hint);
    NAOS_CHECK_STR_EQ(expected_cat_help, help);
    mnd_args_text_free(help);
}

NAOS_TEST(output_uses_lf_only_and_no_trailing_spaces)
{
    const mnd_args_command_t *root = test_tool_program.root;
    mnd_args_result_t result;
    char *text = NULL;
    size_t i;
    size_t k;

    for (i = 0; i <= root->command_count; i++)
    {
        const mnd_args_command_t *command = NULL;

        if (i < root->command_count)
        {
            command = &root->commands[i];
        }
        make_path(&result, command);
        text = mnd_args_format_help(&result);
        mnd_args_result_free(&result);
        NAOS_REQUIRE_NOT_NULL(text);
        NAOS_REQUIRE_UINT_GT(strlen(text), 0);
        NAOS_CHECK_INT_EQ('\n', text[strlen(text) - 1]);
        for (k = 0; k < strlen(text); k++)
        {
            NAOS_CHECK_INT_NE('\r', text[k]);
            if (text[k] == '\n')
            {
                NAOS_CHECK_TRUE((k == 0) || (text[k - 1] != ' '));
            }
        }
        mnd_args_text_free(text);
        text = NULL;
    }
}

NAOS_SUITE(help,
    NAOS_ADD_TEST(global_help_matches_expected_text),
    NAOS_ADD_TEST(build_help_shows_options_annotations_and_list_values),
    NAOS_ADD_TEST(copy_help_uses_summary_and_required_positionals),
    NAOS_ADD_TEST(cat_help_shows_optional_unlimited_positional),
    NAOS_ADD_TEST(version_is_program_and_version_line),
    NAOS_ADD_TEST(usage_hint_without_command),
    NAOS_ADD_TEST(usage_hint_with_command),
    NAOS_ADD_TEST(generating_twice_gives_identical_text),
    NAOS_ADD_TEST(consecutive_texts_are_independent),
    NAOS_ADD_TEST(output_uses_lf_only_and_no_trailing_spaces)
)
