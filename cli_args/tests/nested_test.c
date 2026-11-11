#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/help.h"
#include "clopts/errors.h"
#include "test_decl.h"

/* 入れ子の構成（nest）の経路とエラーのメッセージの接頭辞（2.2、2.3、2.4、2.8、6.4、7.1、7.2、8.9） */

#define NESTED_MAX_ARGS 12
#define NESTED_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct nested_run
{
    char storage[NESTED_MAX_ARGS][NESTED_MAX_LEN];
    char *argv[NESTED_MAX_ARGS + 1];
    mnd_args_result_t result;
    clopts_errors_t errors;
    mnd_args_status_t status;
} nested_run_t;

/* tokens の argc 個を引数列として test_nest_program で解析する */
static void run_parse(nested_run_t *run, int argc, const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    mnd_args_result_init(&run->result);
    clopts_errors_init(&run->errors);

    NAOS_REQUIRE_TRUE(argc <= NESTED_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < NESTED_MAX_LEN);
        strcpy(run->storage[i], tokens[i]);
        run->argv[i] = run->storage[i];
    }
    run->argv[argc] = NULL;

    run->status = mnd_args_parse(&test_nest_program, argc, run->argv, &run->result, &run->errors);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
}

static void run_free(nested_run_t *run)
{
    mnd_args_result_free(&run->result);
    clopts_errors_free(&run->errors);
}

/* エラーなしで status になり、経路の段の数が depth で最後が command であることを確かめる */
static void check_status(const nested_run_t *run, mnd_args_status_t status, size_t depth,
                         const mnd_args_command_t *command)
{
    NAOS_CHECK_INT_EQ(status, run->status);
    NAOS_CHECK_UINT_EQ(depth, mnd_args_depth(&run->result));
    NAOS_CHECK_PTR_EQ(command, mnd_args_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run->errors));
}

/*
 * ヘルプの期待する文面。設計の書式から手で導いたもの。列の揃えは段落ごとに左列の最大幅を求め、
 * 説明を 2 + 最大幅 + 2 バイト目から始める
 */

/*
 * ルートコマンド: オプションがあるので使い方の行の経路の直後に " [<options>]"。
 * Commands: 最大幅は "remote" の 6。Options: 最大幅は "-c, --config <FILE>" の 19
 */
static const char expected_root_help[] =
    "Usage: nest [<options>] <command> [<options>] [<arguments>]\n"
    "\n"
    "A sample tool with nested commands.\n"
    "\n"
    "Commands:\n"
    "  remote  Manage remotes\n"
    "  status  Show the status\n"
    "  help    Show help for a command\n"
    "\n"
    "Options:\n"
    "  -v, --verbose        Print more details\n"
    "  -c, --config <FILE>  Read settings from FILE (default: nest.cfg)\n"
    "      --dry-run        Show what would be done\n"
    "  -h, --help           Show this help\n"
    "      --version        Show version information\n"
    "\n"
    "Run 'nest <command> --help' for help on a command.\n";

/*
 * 子を持つ 2 段目のコマンド: 自分のオプションの後にルートの引き継ぐオプション（--dry-run は出ない）。
 * --version の行はない。Options: 最大幅は "    --tags <TAG>[,<TAG>...]" の 27
 */
static const char expected_remote_help[] =
    "Usage: nest remote [<options>] <command> [<options>] [<arguments>]\n"
    "\n"
    "Manage remotes\n"
    "\n"
    "Commands:\n"
    "  add     Add a remote\n"
    "  remove  Remove a remote\n"
    "  help    Show help for a command\n"
    "\n"
    "Options:\n"
    "  -t, --token <TOKEN>          Access token (required)\n"
    "      --tags <TAG>[,<TAG>...]  Tags to attach\n"
    "  -v, --verbose                Print more details\n"
    "  -c, --config <FILE>          Read settings from FILE (default: nest.cfg)\n"
    "  -h, --help                   Show this help\n"
    "\n"
    "Run 'nest remote <command> --help' for help on a command.\n";

/* 位置引数もオプションも持たない末端コマンド: Arguments: の段落を出さず、引き継いだオプションを並べる */
static const char expected_status_help[] =
    "Usage: nest status [<options>]\n"
    "\n"
    "Show the status\n"
    "\n"
    "Options:\n"
    "  -v, --verbose        Print more details\n"
    "  -c, --config <FILE>  Read settings from FILE (default: nest.cfg)\n"
    "  -h, --help           Show this help\n";

/* 生成した文面を期待する文面とバイト単位で比べる */
static void check_text(const char *expected, const char *text)
{
    NAOS_CHECK_STR_EQ(expected, text);
    NAOS_CHECK_MEM_EQ(expected, strlen(expected), text, strlen(text));
}

/* 解析結果の経路のヘルプを生成し、期待する文面と比べる。2 回生成して同じ文面になることも確かめる（9.9） */
static void check_help_text(const nested_run_t *run, const char *expected)
{
    char *first = NULL;
    char *second = NULL;

    first = mnd_args_format_help(&run->result);
    NAOS_REQUIRE_NOT_NULL(first);
    second = mnd_args_format_help(&run->result);
    NAOS_REQUIRE_NOT_NULL(second);
    check_text(expected, first);
    NAOS_CHECK_MEM_EQ(first, strlen(first), second, strlen(second));

    mnd_args_text_free(first);
    mnd_args_text_free(second);
}

/*
 * 使い方のエラーで、経路の最後が command で、期待するメッセージのエラーがちょうど 1 件だけ
 * 追加されたことを確かめる（7.2、7.4）
 */
static void check_usage_error(const nested_run_t *run, const mnd_args_command_t *command,
                              const char *expected)
{
    const char *message;

    NAOS_CHECK_INT_EQ(MND_ARGS_USAGE_ERROR, run->status);
    NAOS_CHECK_PTR_EQ(command, mnd_args_command(&run->result));
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&run->errors));
    message = clopts_errors_at(&run->errors, 0);
    NAOS_CHECK_STR_EQ(expected, message);
}

/* 経路（2.2、2.8） */

NAOS_TEST(remote_add_path_has_three_levels)
{
    static const char *const tokens[] = {"nest", "remote", "add", "n", "u", "-t", "x"};
    nested_run_t run;

    run_parse(&run, 7, tokens);
    check_status(&run, MND_ARGS_OK, 3, test_nest_remote_add);
    NAOS_CHECK_PTR_EQ(test_nest_program.root, mnd_args_command_at(&run.result, 0));
    NAOS_CHECK_PTR_EQ(test_nest_remote, mnd_args_command_at(&run.result, 1));
    NAOS_CHECK_PTR_EQ(test_nest_remote_add, mnd_args_command_at(&run.result, 2));
    NAOS_CHECK_STR_EQ("remote", mnd_args_command_at(&run.result, 1)->name);
    NAOS_CHECK_STR_EQ("add", mnd_args_command_at(&run.result, 2)->name);
    NAOS_REQUIRE_UINT_EQ(2, mnd_args_positional_count(&run.result));
    NAOS_CHECK_PTR_EQ(run.argv[3], mnd_args_positional_at(&run.result, 0));
    NAOS_CHECK_PTR_EQ(run.argv[4], mnd_args_positional_at(&run.result, 1));
    NAOS_CHECK_PTR_EQ(run.argv[6], mnd_args_value(&run.result, "token"));
    NAOS_CHECK_FALSE(mnd_args_flag(&run.result, "force"));
    run_free(&run);
}

NAOS_TEST(remote_remove_path_has_three_levels)
{
    static const char *const tokens[] = {"nest", "remote", "remove", "n", "--token", "x"};
    nested_run_t run;

    run_parse(&run, 6, tokens);
    check_status(&run, MND_ARGS_OK, 3, test_nest_remote_remove);
    NAOS_CHECK_PTR_EQ(test_nest_remote, mnd_args_command_at(&run.result, 1));
    NAOS_REQUIRE_UINT_EQ(1, mnd_args_positional_count(&run.result));
    NAOS_CHECK_STR_EQ("n", mnd_args_positional_at(&run.result, 0));
    NAOS_CHECK_STR_EQ("x", mnd_args_value(&run.result, "token"));
    run_free(&run);
}

/* 子を持つ 2 段目のコマンドで引数列が終わると子コマンドの未指定（2.4） */
NAOS_TEST(remote_without_child_is_no_command)
{
    static const char *const tokens[] = {"nest", "remote"};
    nested_run_t run;

    run_parse(&run, 2, tokens);
    check_status(&run, MND_ARGS_NO_COMMAND, 2, test_nest_remote);
    run_free(&run);
}

/* 必須の --token は経路にないので検査されない（7.1） */
NAOS_TEST(status_is_ok)
{
    static const char *const tokens[] = {"nest", "status"};
    nested_run_t run;

    run_parse(&run, 2, tokens);
    check_status(&run, MND_ARGS_OK, 2, test_nest_status);
    NAOS_CHECK_UINT_EQ(0, mnd_args_positional_count(&run.result));
    NAOS_CHECK_NULL(mnd_args_value(&run.result, "token"));
    run_free(&run);
}

/* エラーのメッセージの接頭辞は、誤りを検出した時点の経路になる（2.3、6.4、7.2） */

NAOS_TEST(unknown_command_under_remote_has_remote_prefix)
{
    static const char *const tokens[] = {"nest", "remote", "bogus"};
    nested_run_t run;

    run_parse(&run, 3, tokens);
    check_usage_error(&run, test_nest_remote, "nest remote: unknown command 'bogus'");
    NAOS_CHECK_UINT_EQ(2, mnd_args_depth(&run.result));
    run_free(&run);
}

NAOS_TEST(unknown_command_at_root_has_root_prefix)
{
    static const char *const tokens[] = {"nest", "add"};
    nested_run_t run;

    run_parse(&run, 2, tokens);
    check_usage_error(&run, test_nest_program.root, "nest: unknown command 'add'");
    NAOS_CHECK_UINT_EQ(1, mnd_args_depth(&run.result));
    run_free(&run);
}

NAOS_TEST(missing_argument_has_remote_add_prefix)
{
    static const char *const tokens[] = {"nest", "remote", "add", "n"};
    nested_run_t run;

    run_parse(&run, 4, tokens);
    check_usage_error(&run, test_nest_remote_add, "nest remote add: missing argument <URL>");
    NAOS_CHECK_UINT_EQ(3, mnd_args_depth(&run.result));
    run_free(&run);
}

NAOS_TEST(unexpected_argument_has_remote_add_prefix)
{
    static const char *const tokens[] = {"nest", "remote", "add", "n", "u", "v", "-t", "x"};
    nested_run_t run;

    run_parse(&run, 8, tokens);
    check_usage_error(&run, test_nest_remote_add, "nest remote add: unexpected argument 'v'");
    run_free(&run);
}

NAOS_TEST(unknown_option_on_status_has_status_prefix)
{
    static const char *const tokens[] = {"nest", "status", "--force"};
    nested_run_t run;

    run_parse(&run, 3, tokens);
    check_usage_error(&run, test_nest_status, "nest status: unknown option '--force'");
    run_free(&run);
}

/* --version はルートコマンドが選択中の間だけ有効（8.9） */
NAOS_TEST(version_under_remote_is_unknown_option)
{
    static const char *const tokens[] = {"nest", "remote", "--version"};
    nested_run_t run;

    run_parse(&run, 3, tokens);
    check_usage_error(&run, test_nest_remote, "nest remote: unknown option '--version'");
    run_free(&run);
}

/* help の語は任意の深さの子コマンド名をたどる（8.1、8.4、8.5） */

NAOS_TEST(help_word_reaches_nested_command)
{
    static const char *const from_root[] = {"nest", "help", "remote", "add"};
    static const char *const from_remote[] = {"nest", "remote", "help", "add"};
    static const char *const long_help[] = {"nest", "remote", "add", "--help"};
    nested_run_t run;

    run_parse(&run, 4, from_root);
    check_status(&run, MND_ARGS_HELP, 3, test_nest_remote_add);
    NAOS_CHECK_PTR_EQ(test_nest_remote, mnd_args_command_at(&run.result, 1));
    run_free(&run);

    run_parse(&run, 4, from_remote);
    check_status(&run, MND_ARGS_HELP, 3, test_nest_remote_add);
    NAOS_CHECK_PTR_EQ(test_nest_remote, mnd_args_command_at(&run.result, 1));
    run_free(&run);

    run_parse(&run, 4, long_help);
    check_status(&run, MND_ARGS_HELP, 3, test_nest_remote_add);
    run_free(&run);
}

NAOS_TEST(help_word_reaches_command_with_children)
{
    static const char *const tokens[] = {"nest", "help", "remote"};
    nested_run_t run;

    run_parse(&run, 3, tokens);
    check_status(&run, MND_ARGS_HELP, 2, test_nest_remote);
    run_free(&run);
}

NAOS_TEST(help_word_under_remote_alone_is_remote_help)
{
    static const char *const tokens[] = {"nest", "remote", "help"};
    nested_run_t run;

    run_parse(&run, 3, tokens);
    check_status(&run, MND_ARGS_HELP, 2, test_nest_remote);
    run_free(&run);
}

/* help の後の名前の誤りは、段を help を書いたコマンドまで戻して報告する（8.6） */

NAOS_TEST(help_word_unknown_nested_name_has_root_prefix)
{
    static const char *const tokens[] = {"nest", "help", "remote", "bogus"};
    nested_run_t run;

    run_parse(&run, 4, tokens);
    check_usage_error(&run, test_nest_program.root, "nest: unknown command 'bogus'");
    NAOS_CHECK_UINT_EQ(1, mnd_args_depth(&run.result));
    run_free(&run);
}

NAOS_TEST(help_word_extra_argument_after_leaf_has_root_prefix)
{
    static const char *const tokens[] = {"nest", "help", "status", "x"};
    nested_run_t run;

    run_parse(&run, 4, tokens);
    check_usage_error(&run, test_nest_program.root, "nest: unexpected argument 'x'");
    NAOS_CHECK_UINT_EQ(1, mnd_args_depth(&run.result));
    run_free(&run);
}

NAOS_TEST(help_word_error_under_remote_has_remote_prefix)
{
    static const char *const tokens[] = {"nest", "remote", "help", "add", "x", "--help"};
    nested_run_t run;

    run_parse(&run, 6, tokens);
    check_usage_error(&run, test_nest_remote, "nest remote: unexpected argument 'x'");
    NAOS_CHECK_UINT_EQ(2, mnd_args_depth(&run.result));
    run_free(&run);
}

/* ヘルプの優先は末端コマンドが選択中の誤りに限る（8.3、8.10） */

NAOS_TEST(unknown_command_under_remote_before_help_is_reported)
{
    static const char *const tokens[] = {"nest", "remote", "bogus", "--help"};
    nested_run_t run;

    run_parse(&run, 4, tokens);
    check_usage_error(&run, test_nest_remote, "nest remote: unknown command 'bogus'");
    run_free(&run);
}

NAOS_TEST(error_on_remote_add_before_help_is_add_help)
{
    static const char *const tokens[] = {"nest", "remote", "add", "--bad", "--help"};
    nested_run_t run;

    run_parse(&run, 5, tokens);
    check_status(&run, MND_ARGS_HELP, 3, test_nest_remote_add);
    run_free(&run);
}

/* ヘルプと案内の文面（2.4、7.3、9.1、9.2、9.4、9.5、9.9） */

NAOS_TEST(root_help_text_matches_expected_text)
{
    static const char *const tokens[] = {"nest", "--help"};
    nested_run_t run;

    run_parse(&run, 2, tokens);
    check_status(&run, MND_ARGS_HELP, 1, test_nest_program.root);
    check_help_text(&run, expected_root_help);
    run_free(&run);
}

/* 子コマンドの未指定の結果からも、同じ子を持つコマンドのヘルプを生成できる（2.4） */
NAOS_TEST(remote_help_text_matches_expected_text)
{
    static const char *const no_command[] = {"nest", "remote"};
    static const char *const long_help[] = {"nest", "remote", "--help"};
    nested_run_t run;

    run_parse(&run, 2, no_command);
    check_status(&run, MND_ARGS_NO_COMMAND, 2, test_nest_remote);
    check_help_text(&run, expected_remote_help);
    run_free(&run);

    run_parse(&run, 3, long_help);
    check_status(&run, MND_ARGS_HELP, 2, test_nest_remote);
    check_help_text(&run, expected_remote_help);
    run_free(&run);
}

NAOS_TEST(status_help_text_matches_expected_text)
{
    static const char *const tokens[] = {"nest", "status", "--help"};
    nested_run_t run;

    run_parse(&run, 3, tokens);
    check_status(&run, MND_ARGS_HELP, 2, test_nest_status);
    check_help_text(&run, expected_status_help);
    run_free(&run);
}

NAOS_TEST(usage_hint_has_remote_path)
{
    static const char *const tokens[] = {"nest", "remote", "bogus"};
    nested_run_t run;
    char *text = NULL;

    run_parse(&run, 3, tokens);
    check_usage_error(&run, test_nest_remote, "nest remote: unknown command 'bogus'");
    text = mnd_args_format_usage_hint(&run.result);
    NAOS_REQUIRE_NOT_NULL(text);
    check_text("Run 'nest remote --help' for usage.\n", text);

    mnd_args_text_free(text);
    run_free(&run);
}

NAOS_TEST(path_has_all_levels)
{
    static const char *const tokens[] = {"nest", "remote", "add", "n"};
    nested_run_t run;
    char *text = NULL;

    run_parse(&run, 4, tokens);
    text = mnd_args_format_path(&run.result);
    NAOS_REQUIRE_NOT_NULL(text);
    check_text("nest remote add", text);

    mnd_args_text_free(text);
    run_free(&run);
}

NAOS_SUITE(nested,
    NAOS_ADD_TEST(remote_add_path_has_three_levels),
    NAOS_ADD_TEST(remote_remove_path_has_three_levels),
    NAOS_ADD_TEST(remote_without_child_is_no_command),
    NAOS_ADD_TEST(status_is_ok),
    NAOS_ADD_TEST(unknown_command_under_remote_has_remote_prefix),
    NAOS_ADD_TEST(unknown_command_at_root_has_root_prefix),
    NAOS_ADD_TEST(missing_argument_has_remote_add_prefix),
    NAOS_ADD_TEST(unexpected_argument_has_remote_add_prefix),
    NAOS_ADD_TEST(unknown_option_on_status_has_status_prefix),
    NAOS_ADD_TEST(version_under_remote_is_unknown_option),
    NAOS_ADD_TEST(help_word_reaches_nested_command),
    NAOS_ADD_TEST(help_word_reaches_command_with_children),
    NAOS_ADD_TEST(help_word_under_remote_alone_is_remote_help),
    NAOS_ADD_TEST(help_word_unknown_nested_name_has_root_prefix),
    NAOS_ADD_TEST(help_word_extra_argument_after_leaf_has_root_prefix),
    NAOS_ADD_TEST(help_word_error_under_remote_has_remote_prefix),
    NAOS_ADD_TEST(unknown_command_under_remote_before_help_is_reported),
    NAOS_ADD_TEST(error_on_remote_add_before_help_is_add_help),
    NAOS_ADD_TEST(root_help_text_matches_expected_text),
    NAOS_ADD_TEST(remote_help_text_matches_expected_text),
    NAOS_ADD_TEST(status_help_text_matches_expected_text),
    NAOS_ADD_TEST(usage_hint_has_remote_path),
    NAOS_ADD_TEST(path_has_all_levels)
)
