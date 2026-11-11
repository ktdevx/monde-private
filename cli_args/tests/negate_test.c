#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "clopts/help.h"
#include "clopts_internal.h"
#include "test_decl.h"

/* 否定できるフラグ（--no-<name>）の解析（14.1〜14.9）。プログラム neg を使う（11.1） */

#define NEGATE_MAX_ARGS 14
#define NEGATE_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct negate_run
{
    char storage[NEGATE_MAX_ARGS][NEGATE_MAX_LEN];
    char *argv[NEGATE_MAX_ARGS + 1];
    clopts_result_t result;
    clopts_errors_t errors;
    clopts_status_t status;
} negate_run_t;

/* tokens の argc 個を引数列として test_neg_program で解析する */
static void run_parse(negate_run_t *run, int argc, const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    clopts_result_init(&run->result);
    clopts_errors_init(&run->errors);

    NAOS_REQUIRE_TRUE(argc <= NEGATE_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < NEGATE_MAX_LEN);
        strcpy(run->storage[i], tokens[i]);
        run->argv[i] = run->storage[i];
    }
    run->argv[argc] = NULL;

    run->status = clopts_parse(&test_neg_program, argc, run->argv, &run->result, &run->errors);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
}

static void run_free(negate_run_t *run)
{
    clopts_result_free(&run->result);
    clopts_errors_free(&run->errors);
}

/* エラーなしで成功し、経路の最後が command であることを確かめる */
static void check_ok(const negate_run_t *run, const clopts_command_t *command)
{
    NAOS_CHECK_INT_EQ(CLOPTS_OK, run->status);
    NAOS_CHECK_PTR_EQ(command, clopts_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run->errors));
}

/* 使い方のエラーで、経路の最後が command で、期待するメッセージのエラーがちょうど 1 件であることを確かめる */
static void check_usage_error(const negate_run_t *run, const clopts_command_t *command,
                              const char *expected)
{
    NAOS_CHECK_INT_EQ(CLOPTS_USAGE_ERROR, run->status);
    NAOS_CHECK_PTR_EQ(command, clopts_command(&run->result));
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&run->errors));
    NAOS_CHECK_STR_EQ(expected, clopts_errors_at(&run->errors, 0));
}

/* tokens を解析して成功し、経路の最後が command で、フラグ long_name の状態が (enabled, set) であることを確かめる */
static void check_flag_state(int argc, const char *const *tokens, const clopts_command_t *command,
                             const char *long_name, bool enabled, bool set)
{
    negate_run_t run;

    run_parse(&run, argc, tokens);
    check_ok(&run, command);
    NAOS_CHECK_INT_EQ(enabled, clopts_flag(&run.result, long_name));
    NAOS_CHECK_INT_EQ(set, clopts_is_set(&run.result, long_name));
    run_free(&run);
}

/* tokens を解析して、command で expected の使い方のエラーになることを確かめる */
static void check_error(int argc, const char *const *tokens, const clopts_command_t *command,
                        const char *expected)
{
    negate_run_t run;

    run_parse(&run, argc, tokens);
    check_usage_error(&run, command, expected);
    run_free(&run);
}

/* 宣言 */

NAOS_TEST(neg_declaration_is_valid)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    NAOS_CHECK_TRUE(clopts_decl_check(&test_neg_program, &errors));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&errors));
    clopts_errors_free(&errors);
}

/* 肯定の形と未指定の状態（14.5、14.6） */

NAOS_TEST(positive_form_enables_negatable_flag)
{
    static const char *const tokens[] = {"neg", "--color", "run"};
    negate_run_t run;

    run_parse(&run, 3, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "color"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "color"));
    run_free(&run);
}

NAOS_TEST(unset_negatable_flag_is_disabled_and_not_set)
{
    static const char *const tokens[] = {"neg", "run"};
    negate_run_t run;

    run_parse(&run, 2, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "color"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "color"));
    run_free(&run);
}

/*
 * ヘルプ（9.5、11.4、14.12、14.13、14.17）。期待する文面は設計の書式から手で導いたもの。
 * 否定できるフラグは左列の "--" の後ろに "[no-]" を挟み、その 5 バイトを左列の幅に含める。
 * 既定で有効のフラグだけ、説明に " (default: on)" を付ける（説明がなければ先頭の空白を出さない）
 */

/* ルートコマンド。Commands: 最大幅は "help" の 4。Options: 最大幅は "    --name <NAME>" の 17 */
static const char expected_neg_root_help[] =
    "Usage: neg [<options>] <command> [<options>] [<arguments>]\n"
    "\n"
    "A sample tool with negatable flags.\n"
    "\n"
    "Commands:\n"
    "  run   Process files\n"
    "  help  Show help for a command\n"
    "\n"
    "Options:\n"
    "  -c, --[no-]color   Colorize the output\n"
    "      --[no-]pager   (default: on)\n"
    "      --no-cache     Do not use the cache\n"
    "      --wrap         Wrap long lines\n"
    "      --name <NAME>  Name of the job\n"
    "  -h, --help         Show this help\n"
    "      --version      Show version information\n"
    "\n"
    "Run 'neg <command> --help' for help on a command.\n";

/*
 * run。Options: は run のオプション、ルートから引き継いだ --[no-]color と --[no-]pager、-h, --help の順。
 * 最大幅は "-f, --[no-]force" と "-c, --[no-]color" と "    --[no-]pager" の 16
 */
static const char expected_neg_run_help[] =
    "Usage: neg run [<options>] [<FILE>...]\n"
    "\n"
    "Process files\n"
    "\n"
    "Arguments:\n"
    "  <FILE>  Files to process\n"
    "\n"
    "Options:\n"
    "  -f, --[no-]force  Overwrite existing files\n"
    "      --[no-]sync   Flush after each file (default: on)\n"
    "  -q, --quiet       Print only errors\n"
    "  -c, --[no-]color  Colorize the output\n"
    "      --[no-]pager  (default: on)\n"
    "  -h, --help        Show this help\n";

/* tokens を解析してヘルプの要求になることを確かめ、生成したヘルプを expected とバイト単位で比較する */
static void check_help(int argc, const char *const *tokens, const clopts_command_t *command,
                       const char *expected)
{
    negate_run_t run;
    char *text = NULL;

    run_parse(&run, argc, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_HELP, run.status);
    NAOS_CHECK_PTR_EQ(command, clopts_command(&run.result));
    text = clopts_format_help(&run.result);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_CHECK_STR_EQ(expected, text);
    NAOS_CHECK_MEM_EQ(expected, strlen(expected), text, strlen(text));

    clopts_text_free(text);
    run_free(&run);
}

NAOS_TEST(root_help_shows_negatable_flags_and_default_on)
{
    static const char *const tokens[] = {"neg", "--help"};

    check_help(2, tokens, test_neg_program.root, expected_neg_root_help);
}

NAOS_TEST(run_help_shows_inherited_negatable_flags)
{
    static const char *const tokens[] = {"neg", "run", "--help"};

    check_help(3, tokens, test_neg_run, expected_neg_run_help);
}

/* 否定の形（14.2、14.5） */

NAOS_TEST(negated_form_disables_negatable_flag)
{
    static const char *const tokens[] = {"neg", "--no-color", "run"};

    check_flag_state(3, tokens, test_neg_run, "color", false, true);
}

NAOS_TEST(negated_form_disables_flag_of_selected_command)
{
    static const char *const tokens[] = {"neg", "run", "--no-force"};

    check_flag_state(3, tokens, test_neg_run, "force", false, true);
}

/* 後に書いた指定の優先（14.4） */

NAOS_TEST(negated_form_after_positive_form_wins)
{
    static const char *const tokens[] = {"neg", "--color", "--no-color", "run"};

    check_flag_state(4, tokens, test_neg_run, "color", false, true);
}

NAOS_TEST(positive_form_after_negated_form_wins)
{
    static const char *const tokens[] = {"neg", "--no-color", "--color", "run"};

    check_flag_state(4, tokens, test_neg_run, "color", true, true);
}

NAOS_TEST(short_form_after_negated_form_wins)
{
    static const char *const tokens[] = {"neg", "--no-color", "-c", "run"};

    check_flag_state(4, tokens, test_neg_run, "color", true, true);
}

NAOS_TEST(negated_form_after_short_group_wins)
{
    static const char *const tokens[] = {"neg", "run", "-fq", "--no-force"};
    negate_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "force"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "force"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "quiet"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "quiet"));
    run_free(&run);
}

NAOS_TEST(short_group_after_negated_form_wins)
{
    static const char *const tokens[] = {"neg", "run", "--no-force", "-qf"};
    negate_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "force"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "force"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "quiet"));
    run_free(&run);
}

/* 引き継ぐフラグ（14.9）。子コマンド名の前後にまたがっても、引数列で最後の指定が残る */

NAOS_TEST(inherited_negated_form_after_command_wins)
{
    static const char *const tokens[] = {"neg", "--color", "run", "--no-color"};

    check_flag_state(4, tokens, test_neg_run, "color", false, true);
}

NAOS_TEST(inherited_positive_form_after_command_wins)
{
    static const char *const tokens[] = {"neg", "--no-color", "run", "--color"};

    check_flag_state(4, tokens, test_neg_run, "color", true, true);
}

NAOS_TEST(inherited_negated_form_in_child_command)
{
    static const char *const tokens[] = {"neg", "run", "--no-color"};

    check_flag_state(3, tokens, test_neg_run, "color", false, true);
}

NAOS_TEST(inherited_negated_form_in_child_command_for_default_on_flag)
{
    static const char *const tokens[] = {"neg", "run", "--no-pager"};

    check_flag_state(3, tokens, test_neg_run, "pager", false, true);
}

NAOS_TEST(negated_form_before_command_for_default_on_flag)
{
    static const char *const tokens[] = {"neg", "--no-pager", "run"};

    check_flag_state(3, tokens, test_neg_run, "pager", false, true);
}

NAOS_TEST(negated_form_for_default_on_flag_of_selected_command)
{
    static const char *const tokens[] = {"neg", "run", "--no-sync"};

    check_flag_state(3, tokens, test_neg_run, "sync", false, true);
}

/* 既定で有効（14.15、14.4、12.6）。指定がなければ有効で、明示的な指定はないものとする */

NAOS_TEST(default_on_flags_are_enabled_and_not_set)
{
    static const char *const tokens[] = {"neg", "run"};
    negate_run_t run;

    run_parse(&run, 2, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "sync"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "sync"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "pager"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "pager"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "force"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "quiet"));
    run_free(&run);
}

NAOS_TEST(default_on_inherited_flag_is_enabled_at_root_level)
{
    static const char *const tokens[] = {"neg", "run"};
    negate_run_t run;

    run_parse(&run, 2, tokens);
    check_ok(&run, test_neg_run);
    NAOS_REQUIRE_UINT_EQ(2, run.result.level_count);
    NAOS_REQUIRE_NOT_NULL(run.result.levels[0].values);
    /* pager はルートの 2 番目のオプション。引き継ぐフラグの値はルートの段に記録される */
    NAOS_CHECK_TRUE(run.result.levels[0].values[1].enabled);
    NAOS_CHECK_FALSE(run.result.levels[0].values[1].explicit_set);
    run_free(&run);
}

NAOS_TEST(positive_form_of_default_on_flag_is_enabled_and_set)
{
    static const char *const tokens[] = {"neg", "run", "--sync"};

    check_flag_state(3, tokens, test_neg_run, "sync", true, true);
}

NAOS_TEST(positive_form_after_negated_form_of_default_on_flag_wins)
{
    static const char *const tokens[] = {"neg", "run", "--no-sync", "--sync"};

    check_flag_state(4, tokens, test_neg_run, "sync", true, true);
}

NAOS_TEST(negated_form_after_positive_form_of_default_on_flag_wins)
{
    static const char *const tokens[] = {"neg", "--pager", "run", "--no-pager"};

    check_flag_state(4, tokens, test_neg_run, "pager", false, true);
}

NAOS_TEST(default_on_flag_is_unaffected_by_other_flag)
{
    static const char *const tokens[] = {"neg", "run", "--no-sync"};
    negate_run_t run;

    run_parse(&run, 3, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "sync"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "pager"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "pager"));
    run_free(&run);
}

/* 完全一致の優先（14.3） */

NAOS_TEST(exact_match_of_no_prefixed_name_wins)
{
    static const char *const tokens[] = {"neg", "--no-cache", "run"};

    check_flag_state(3, tokens, test_neg_run, "no-cache", true, true);
}

/* 値の指定（14.7） */

NAOS_TEST(negated_form_with_value_is_error)
{
    static const char *const tokens[] = {"neg", "--no-color=1", "run"};

    check_error(3, tokens, test_neg_program.root, "neg: option '--no-color' does not take a value");
}

NAOS_TEST(negated_form_with_empty_value_is_error)
{
    static const char *const tokens[] = {"neg", "run", "--no-force="};

    check_error(3, tokens, test_neg_run, "neg run: option '--no-force' does not take a value");
}

/* 未知のオプション（14.8、3.6、3.7） */

NAOS_TEST(negated_form_of_non_negatable_flag_is_unknown)
{
    static const char *const tokens[] = {"neg", "--no-wrap", "run"};

    check_error(3, tokens, test_neg_program.root, "neg: unknown option '--no-wrap'");
}

NAOS_TEST(negated_form_of_value_option_is_unknown)
{
    static const char *const tokens[] = {"neg", "--no-name", "run"};

    check_error(3, tokens, test_neg_program.root, "neg: unknown option '--no-name'");
}

NAOS_TEST(negated_form_of_unselected_child_flag_is_unknown)
{
    static const char *const tokens[] = {"neg", "--no-force", "run"};

    check_error(3, tokens, test_neg_program.root, "neg: unknown option '--no-force'");
}

NAOS_TEST(negated_form_of_nonexistent_name_is_unknown)
{
    static const char *const tokens[] = {"neg", "--no-bogus", "run"};

    check_error(3, tokens, test_neg_program.root, "neg: unknown option '--no-bogus'");
}

NAOS_TEST(no_prefix_alone_is_unknown)
{
    static const char *const tokens[] = {"neg", "--no-", "run"};

    check_error(3, tokens, test_neg_program.root, "neg: unknown option '--no-'");
}

NAOS_TEST(negated_form_of_prefix_is_unknown)
{
    static const char *const tokens[] = {"neg", "--no-colo", "run"};

    check_error(3, tokens, test_neg_program.root, "neg: unknown option '--no-colo'");
}

NAOS_TEST(negated_form_with_different_case_is_unknown)
{
    static const char *const tokens[] = {"neg", "--No-color", "run"};

    check_error(3, tokens, test_neg_program.root, "neg: unknown option '--No-color'");
}

NAOS_TEST(negated_form_of_non_negatable_flag_of_selected_command_is_unknown)
{
    static const char *const tokens[] = {"neg", "run", "--no-quiet=1"};

    check_error(3, tokens, test_neg_run, "neg run: unknown option '--no-quiet'");
}

/* ほかの規則との組み合わせ（6.3、8.3） */

NAOS_TEST(negated_form_after_separator_is_positional)
{
    static const char *const tokens[] = {"neg", "run", "--", "--no-force"};
    negate_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run, test_neg_run);
    NAOS_REQUIRE_UINT_EQ(1, clopts_positional_count(&run.result));
    NAOS_CHECK_PTR_EQ(run.argv[3], clopts_positional_at(&run.result, 0));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "force"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "force"));
    run_free(&run);
}

NAOS_TEST(help_after_unknown_negated_form_wins)
{
    static const char *const tokens[] = {"neg", "run", "--no-quiet", "--help"};

    check_help(4, tokens, test_neg_run, expected_neg_run_help);
}

NAOS_SUITE(negate,
    NAOS_ADD_TEST(neg_declaration_is_valid),
    NAOS_ADD_TEST(positive_form_enables_negatable_flag),
    NAOS_ADD_TEST(unset_negatable_flag_is_disabled_and_not_set),
    NAOS_ADD_TEST(negated_form_disables_negatable_flag),
    NAOS_ADD_TEST(negated_form_disables_flag_of_selected_command),
    NAOS_ADD_TEST(negated_form_after_positive_form_wins),
    NAOS_ADD_TEST(positive_form_after_negated_form_wins),
    NAOS_ADD_TEST(short_form_after_negated_form_wins),
    NAOS_ADD_TEST(negated_form_after_short_group_wins),
    NAOS_ADD_TEST(short_group_after_negated_form_wins),
    NAOS_ADD_TEST(inherited_negated_form_after_command_wins),
    NAOS_ADD_TEST(inherited_positive_form_after_command_wins),
    NAOS_ADD_TEST(inherited_negated_form_in_child_command),
    NAOS_ADD_TEST(inherited_negated_form_in_child_command_for_default_on_flag),
    NAOS_ADD_TEST(negated_form_before_command_for_default_on_flag),
    NAOS_ADD_TEST(negated_form_for_default_on_flag_of_selected_command),
    NAOS_ADD_TEST(default_on_flags_are_enabled_and_not_set),
    NAOS_ADD_TEST(default_on_inherited_flag_is_enabled_at_root_level),
    NAOS_ADD_TEST(positive_form_of_default_on_flag_is_enabled_and_set),
    NAOS_ADD_TEST(positive_form_after_negated_form_of_default_on_flag_wins),
    NAOS_ADD_TEST(negated_form_after_positive_form_of_default_on_flag_wins),
    NAOS_ADD_TEST(default_on_flag_is_unaffected_by_other_flag),
    NAOS_ADD_TEST(exact_match_of_no_prefixed_name_wins),
    NAOS_ADD_TEST(negated_form_with_value_is_error),
    NAOS_ADD_TEST(negated_form_with_empty_value_is_error),
    NAOS_ADD_TEST(negated_form_of_non_negatable_flag_is_unknown),
    NAOS_ADD_TEST(negated_form_of_value_option_is_unknown),
    NAOS_ADD_TEST(negated_form_of_unselected_child_flag_is_unknown),
    NAOS_ADD_TEST(negated_form_of_nonexistent_name_is_unknown),
    NAOS_ADD_TEST(no_prefix_alone_is_unknown),
    NAOS_ADD_TEST(negated_form_of_prefix_is_unknown),
    NAOS_ADD_TEST(negated_form_with_different_case_is_unknown),
    NAOS_ADD_TEST(negated_form_of_non_negatable_flag_of_selected_command_is_unknown),
    NAOS_ADD_TEST(negated_form_after_separator_is_positional),
    NAOS_ADD_TEST(help_after_unknown_negated_form_wins),
    NAOS_ADD_TEST(root_help_shows_negatable_flags_and_default_on),
    NAOS_ADD_TEST(run_help_shows_inherited_negatable_flags)
)
