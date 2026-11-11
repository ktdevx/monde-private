#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "clopts/help.h"
#include "clopts_internal.h"
#include "test_decl.h"

/* 否定できるフラグ（--no-<name>）の解析（14.1、14.5、14.6）。プログラム neg を使う（11.1） */

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

NAOS_SUITE(negate,
    NAOS_ADD_TEST(neg_declaration_is_valid),
    NAOS_ADD_TEST(positive_form_enables_negatable_flag),
    NAOS_ADD_TEST(unset_negatable_flag_is_disabled_and_not_set),
    NAOS_ADD_TEST(root_help_shows_negatable_flags_and_default_on),
    NAOS_ADD_TEST(run_help_shows_inherited_negatable_flags)
)
