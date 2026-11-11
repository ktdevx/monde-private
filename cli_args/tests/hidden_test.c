#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "clopts/help.h"
#include "clopts_internal.h"
#include "test_decl.h"

/*
 * 隠しオプション（16.1〜16.5、16.7、16.8）。プログラム ext と、このファイルの中の小さな宣言 solo を使う（11.1）
 */

#define HIDDEN_MAX_ARGS 14
#define HIDDEN_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct hidden_run
{
    char storage[HIDDEN_MAX_ARGS][HIDDEN_MAX_LEN];
    char *argv[HIDDEN_MAX_ARGS + 1];
    clopts_result_t result;
    clopts_errors_t errors;
    clopts_status_t status;
} hidden_run_t;

/*
 * テスト専用の架空のプログラム "solo"（バージョン 1.0）の宣言。子を持つルートコマンドのオプションが
 * 隠しオプション --dump だけである（16.8）。子コマンド state は末端で、オプションを持たない
 */
static const clopts_option_t solo_root_options[] = {
    { .long_name = "dump", .kind = CLOPTS_FLAG, .help = "Dump the internal state", .hidden = true }
};

static const clopts_command_t solo_commands[] = {
    { .name = "state", .summary = "Show the state" }
};

static const clopts_command_t solo_root = {
    .options = solo_root_options,
    .option_count = sizeof solo_root_options / sizeof solo_root_options[0],
    .commands = solo_commands,
    .command_count = sizeof solo_commands / sizeof solo_commands[0]
};

static const clopts_program_t solo_program = {
    .name = "solo",
    .version = "1.0",
    .root = &solo_root
};

/* tokens の argc 個を引数列として program で解析する */
static void run_parse_program(hidden_run_t *run, const clopts_program_t *program, int argc,
                              const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    clopts_result_init(&run->result);
    clopts_errors_init(&run->errors);

    NAOS_REQUIRE_TRUE(argc <= HIDDEN_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < HIDDEN_MAX_LEN);
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

/* tokens の argc 個を引数列として test_ext_program で解析する */
static void run_parse(hidden_run_t *run, int argc, const char *const *tokens)
{
    run_parse_program(run, &test_ext_program, argc, tokens);
}

static void run_free(hidden_run_t *run)
{
    clopts_result_free(&run->result);
    clopts_errors_free(&run->errors);
}

/* エラーなしで成功し、経路の最後が command であることを確かめる */
static void check_ok(const hidden_run_t *run, const clopts_command_t *command)
{
    NAOS_CHECK_INT_EQ(CLOPTS_OK, run->status);
    NAOS_CHECK_PTR_EQ(command, clopts_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run->errors));
}

/* tokens を解析して、command で expected の使い方のエラーがちょうど 1 件になることを確かめる */
static void check_error(int argc, const char *const *tokens, const clopts_command_t *command,
                        const char *expected)
{
    hidden_run_t run;

    run_parse(&run, argc, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_USAGE_ERROR, run.status);
    NAOS_CHECK_PTR_EQ(command, clopts_command(&run.result));
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&run.errors));
    NAOS_CHECK_STR_EQ(expected, clopts_errors_at(&run.errors, 0));
    run_free(&run);
}

/*
 * program で tokens を解析してヘルプの要求になることを確かめ、生成したヘルプを expected と
 * バイト単位で比較する
 */
static void check_help(const clopts_program_t *program, int argc, const char *const *tokens,
                       const clopts_command_t *command, const char *expected)
{
    hidden_run_t run;
    char *text = NULL;

    run_parse_program(&run, program, argc, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_HELP, run.status);
    NAOS_CHECK_PTR_EQ(command, clopts_command(&run.result));
    text = clopts_format_help(&run.result);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_CHECK_STR_EQ(expected, text);
    NAOS_CHECK_MEM_EQ(expected, strlen(expected), text, strlen(text));

    clopts_text_free(text);
    run_free(&run);
}

/* 宣言 */

NAOS_TEST(ext_declaration_is_valid)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;

    NAOS_CHECK_TRUE(clopts_decl_check(&test_ext_program, &errors));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&errors));
    clopts_errors_free(&errors);
}

/* 解析（16.4）。隠しオプションを元の名前で指定すると、ほかのオプションと同じに記録される */

NAOS_TEST(hidden_options_are_recorded_by_their_names)
{
    static const char *const tokens[] = {"ext", "--trace", "run", "--token", "t", "--jobs", "4"};
    hidden_run_t run;

    run_parse(&run, 7, tokens);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "trace"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "trace"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "jobs"));
    NAOS_CHECK_STR_EQ("4", clopts_value(&run.result, "jobs"));
    NAOS_CHECK_PTR_EQ(run.argv[6], clopts_value(&run.result, "jobs"));
    NAOS_CHECK_STR_EQ("t", clopts_value(&run.result, "token"));
    run_free(&run);
}

NAOS_TEST(hidden_option_takes_its_default_value)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t"};
    hidden_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_STR_EQ("1", clopts_value(&run.result, "jobs"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "jobs"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "trace"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "trace"));
    run_free(&run);
}

NAOS_TEST(hidden_option_is_matched_by_its_short_name)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "-j2"};
    hidden_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "jobs"));
    NAOS_CHECK_STR_EQ("2", clopts_value(&run.result, "jobs"));
    run_free(&run);
}

/* 有効なオプションが隠しオプションだけのコマンドでも、解析は同じである（16.4） */
NAOS_TEST(solo_parses_its_only_hidden_option)
{
    static const char *const tokens[] = {"solo", "--dump", "state"};
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    hidden_run_t run;

    NAOS_CHECK_TRUE(clopts_decl_check(&solo_program, &errors));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&errors));
    clopts_errors_free(&errors);

    run_parse_program(&run, &solo_program, 3, tokens);
    check_ok(&run, &solo_commands[0]);
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "dump"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "dump"));
    run_free(&run);
}

/* エラー（16.5、12.2）。隠しオプションの指定の誤りは、ほかのオプションと同じ文面になる */

NAOS_TEST(hidden_value_option_without_value_is_an_error)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--jobs"};

    check_error(5, tokens, test_ext_run, "ext run: option '--jobs' requires a value");
}

NAOS_TEST(hidden_flag_with_value_is_an_error)
{
    static const char *const tokens[] = {"ext", "--trace=1", "run"};

    check_error(3, tokens, test_ext_program.root, "ext: option '--trace' does not take a value");
}

/* 引き継がない隠しオプションは、子コマンドでは未知のオプションである */
NAOS_TEST(hidden_option_that_is_not_inherited_is_unknown_in_child)
{
    static const char *const tokens[] = {"ext", "run", "--profile", "x"};

    check_error(4, tokens, test_ext_run, "ext run: unknown option '--profile'");
}

/*
 * ヘルプ（9.5、11.4、16.2、16.3、16.7、16.8、17.11）。期待する文面は設計の書式から手で導いたもの。
 * 隠しオプションは一覧に出さず、列幅の基準にも入れない。別名はどこにも出ない
 */

/*
 * ext のルートコマンド。隠しオプション --trace と --profile <NAME> の行がない。隠しオプションではない
 * 有効なオプションがあるので、経路の直後に " [<options>]" が出る。
 * Options: 最大幅は "    --[no-]color" の 16（"    --profile <NAME>" の 20 は基準に入れない）
 */
static const char expected_ext_root_help[] =
    "Usage: ext [<options>] <command> [<options>] [<arguments>]\n"
    "\n"
    "A sample tool with hidden options and aliases.\n"
    "\n"
    "Commands:\n"
    "  run   Process files\n"
    "  help  Show help for a command\n"
    "\n"
    "Options:\n"
    "  -v, --verbose     Print more details\n"
    "      --[no-]color  Colorize the output\n"
    "  -h, --help        Show this help\n"
    "      --version     Show version information\n"
    "\n"
    "Run 'ext <command> --help' for help on a command.\n";

/*
 * run。隠しオプション -j, --jobs <NUMBER-OF-WORKERS> と、ルートから引き継いだ隠しオプション --trace の
 * 行がない。Options: 最大幅は "    --tags <TAG>[,<TAG>...]" の 27
 * （"-j, --jobs <NUMBER-OF-WORKERS>" の 30 は基準に入れない）
 */
static const char expected_ext_run_help[] =
    "Usage: ext run [<options>] [<FILE>...]\n"
    "\n"
    "Process files\n"
    "\n"
    "Arguments:\n"
    "  <FILE>  Files to process\n"
    "\n"
    "Options:\n"
    "  -o, --output <FILE>          Write the result to FILE\n"
    "      --token <TOKEN>          Access token (required)\n"
    "      --tags <TAG>[,<TAG>...]  Tags to attach\n"
    "  -f, --force                  Overwrite existing files\n"
    "  -v, --verbose                Print more details\n"
    "      --[no-]color             Colorize the output\n"
    "  -h, --help                   Show this help\n";

/*
 * solo のルートコマンド。有効なオプションが隠しオプションだけなので、オプションを 1 つも宣言しない
 * コマンドと同じ形になる: 経路の直後に " [<options>]" がなく、Options: は -h, --help と --version だけ
 */
static const char expected_solo_root_help[] =
    "Usage: solo <command> [<options>] [<arguments>]\n"
    "\n"
    "Commands:\n"
    "  state  Show the state\n"
    "  help   Show help for a command\n"
    "\n"
    "Options:\n"
    "  -h, --help     Show this help\n"
    "      --version  Show version information\n"
    "\n"
    "Run 'solo <command> --help' for help on a command.\n";

NAOS_TEST(root_help_omits_hidden_options)
{
    static const char *const tokens[] = {"ext", "--help"};

    check_help(&test_ext_program, 2, tokens, test_ext_program.root, expected_ext_root_help);
}

NAOS_TEST(run_help_omits_hidden_options_and_aligns_without_them)
{
    static const char *const tokens[] = {"ext", "run", "--help"};

    check_help(&test_ext_program, 3, tokens, test_ext_run, expected_ext_run_help);
}

NAOS_TEST(solo_help_is_the_same_as_without_options)
{
    static const char *const tokens[] = {"solo", "--help"};

    check_help(&solo_program, 2, tokens, solo_program.root, expected_solo_root_help);
}

NAOS_SUITE(hidden,
    NAOS_ADD_TEST(ext_declaration_is_valid),
    NAOS_ADD_TEST(hidden_options_are_recorded_by_their_names),
    NAOS_ADD_TEST(hidden_option_takes_its_default_value),
    NAOS_ADD_TEST(hidden_option_is_matched_by_its_short_name),
    NAOS_ADD_TEST(solo_parses_its_only_hidden_option),
    NAOS_ADD_TEST(hidden_value_option_without_value_is_an_error),
    NAOS_ADD_TEST(hidden_flag_with_value_is_an_error),
    NAOS_ADD_TEST(hidden_option_that_is_not_inherited_is_unknown_in_child),
    NAOS_ADD_TEST(root_help_omits_hidden_options),
    NAOS_ADD_TEST(run_help_omits_hidden_options_and_aligns_without_them),
    NAOS_ADD_TEST(solo_help_is_the_same_as_without_options)
)
