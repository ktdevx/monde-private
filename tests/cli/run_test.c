/* スイート run: ヘルプ、バージョン、引数なし、使い方のエラー、終了コード（1.x、9.5〜9.9） */

#include <naos/naos.h>

#include "cli/cli.h"
#include "cli/cli_clopts.h"
#include "cli/cli_commands.h"
#include "cli/cli_internal.h"
#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "clopts/help.h"
#include "foundation/buf.h"
#include "foundation/diag.h"
#include "test_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* 雛形: 終了コードの定数が 0 / 1 / 2 であり、作業ディレクトリのパスが渡されている */
NAOS_TEST(run_scaffold_exit_codes_and_work_dir)
{
    NAOS_CHECK_INT_EQ(0, MND_CLI_EXIT_OK);
    NAOS_CHECK_INT_EQ(1, MND_CLI_EXIT_FAILURE);
    NAOS_CHECK_INT_EQ(2, MND_CLI_EXIT_USAGE);
    NAOS_CHECK_STR_NE("", test_work_dir());
}

/* 補助関数: 作業ディレクトリに書いたバイト列（NUL と CR を含む）がそのまま読み戻せる */
NAOS_TEST(run_util_write_and_read_back_file)
{
    static const char bytes[] = "a\r\nb\0c\n";
    mnd_buf_t content = MND_BUF_INIT;

    NAOS_REQUIRE_TRUE(test_write_file("run_util_write_read.bin", bytes, sizeof bytes - 1));
    NAOS_REQUIRE_TRUE(test_file_exists("run_util_write_read.bin"));
    NAOS_REQUIRE_TRUE(test_read_file("run_util_write_read.bin", &content));
    NAOS_CHECK_MEM_EQ(bytes, sizeof bytes - 1, content.data, content.len);
    NAOS_REQUIRE_TRUE(test_remove_file("run_util_write_read.bin"));
    NAOS_CHECK_FALSE(test_file_exists("run_util_write_read.bin"));
    mnd_buf_free(&content);
}

/* 補助関数: 環境変数の表を引き、表にない名前と表を外した後は NULL を返す */
NAOS_TEST(run_util_getenv_looks_up_table)
{
    static const test_env_var_t vars[] = {
        {"LANG", "ja_JP.UTF-8"},
        {"LC_ALL", ""},
    };

    test_env_set(vars, sizeof vars / sizeof vars[0]);
    NAOS_CHECK_STR_EQ("ja_JP.UTF-8", test_env_getenv("LANG"));
    NAOS_CHECK_STR_EQ("", test_env_getenv("LC_ALL"));
    NAOS_CHECK_NULL(test_env_getenv("LC_MESSAGES"));
    test_env_set(NULL, 0);
    NAOS_CHECK_NULL(test_env_getenv("LANG"));
}

/* 補助関数: 時計は設定した固定時刻を返す */
NAOS_TEST(run_util_clock_returns_fixed_time)
{
    test_clock_set((time_t)1767225600);
    NAOS_CHECK_TRUE(test_clock_now() == (time_t)1767225600);
    NAOS_CHECK_TRUE(test_clock_now() == (time_t)1767225600);
}

/* ---- Commands: 宣言のテーブルとハンドラの対応（2.1） ---- */

/* 設計の Commands の表から手で書いた、期待するオプションの宣言 */
typedef struct expected_option
{
    const char *long_name;
    char short_name;
    mnd_args_kind_t kind;
} expected_option_t;

/* 期待するサブコマンドの宣言（位置引数は min = max = 個数） */
typedef struct expected_command
{
    const char *name;
    const char *positionals[2];
    size_t positional_count;
    const expected_option_t *options;
    size_t option_count;
} expected_command_t;

static const expected_option_t expected_extract_options[] = {
    {MND_CLI_OPT_OUTPUT, 'o', MND_ARGS_VALUE},
    {MND_CLI_OPT_QUIET, 'q', MND_ARGS_FLAG},
    {MND_CLI_OPT_FRONT_MATTER_KEYS, '\0', MND_ARGS_LIST},
    {MND_CLI_OPT_PROJECT_ID_VERSION, '\0', MND_ARGS_VALUE},
    {MND_CLI_OPT_MSGID_BUGS_ADDRESS, '\0', MND_ARGS_VALUE},
    {MND_CLI_OPT_NO_WRAP, '\0', MND_ARGS_FLAG},
};

static const expected_option_t expected_init_options[] = {
    {MND_CLI_OPT_OUTPUT, 'o', MND_ARGS_VALUE},
    {MND_CLI_OPT_QUIET, 'q', MND_ARGS_FLAG},
    {MND_CLI_OPT_LANGUAGE, 'l', MND_ARGS_VALUE},
    {MND_CLI_OPT_NO_WRAP, '\0', MND_ARGS_FLAG},
};

static const expected_option_t expected_update_options[] = {
    {MND_CLI_OPT_OUTPUT, 'o', MND_ARGS_VALUE},
    {MND_CLI_OPT_QUIET, 'q', MND_ARGS_FLAG},
    {MND_CLI_OPT_NO_FUZZY_MATCHING, '\0', MND_ARGS_FLAG},
    {MND_CLI_OPT_NO_WRAP, '\0', MND_ARGS_FLAG},
};

static const expected_option_t expected_translate_options[] = {
    {MND_CLI_OPT_OUTPUT, 'o', MND_ARGS_VALUE},
    {MND_CLI_OPT_QUIET, 'q', MND_ARGS_FLAG},
    {MND_CLI_OPT_FRONT_MATTER_KEYS, '\0', MND_ARGS_LIST},
    {MND_CLI_OPT_USE_FUZZY, '\0', MND_ARGS_FLAG},
};

static const expected_command_t expected_commands[] = {
    {"extract", {"INPUT", NULL}, 1, expected_extract_options,
     sizeof expected_extract_options / sizeof expected_extract_options[0]},
    {"init", {"TEMPLATE", NULL}, 1, expected_init_options,
     sizeof expected_init_options / sizeof expected_init_options[0]},
    {"update", {"TEMPLATE", "PO"}, 2, expected_update_options,
     sizeof expected_update_options / sizeof expected_update_options[0]},
    {"translate", {"INPUT", "PO"}, 2, expected_translate_options,
     sizeof expected_translate_options / sizeof expected_translate_options[0]},
};

#define EXPECTED_COMMAND_COUNT (sizeof expected_commands / sizeof expected_commands[0])

static const mnd_args_command_t *find_command(const mnd_args_program_t *program, const char *name)
{
    size_t i;

    for (i = 0; i < program->root->command_count; i++)
    {
        if (strcmp(program->root->commands[i].name, name) == 0)
        {
            return &program->root->commands[i];
        }
    }

    return NULL;
}

static const mnd_args_option_t *find_option(const mnd_args_command_t *command, const char *long_name)
{
    size_t i;

    for (i = 0; i < command->option_count; i++)
    {
        if (strcmp(command->options[i].long_name, long_name) == 0)
        {
            return &command->options[i];
        }
    }

    return NULL;
}

static bool buf_contains(const mnd_buf_t *buf, const char *needle)
{
    return strstr(mnd_buf_cstr(buf), needle) != NULL;
}

/* 引数列の書き換え可能な複製。解析結果の位置引数と値はこの領域を指すため、結果より長く保持する */
#define ARGV_MAX_ARGS 16
#define ARGV_MAX_LEN 64

typedef struct argv_copy
{
    char storage[ARGV_MAX_ARGS][ARGV_MAX_LEN];
    char *argv[ARGV_MAX_ARGS + 1];
    int argc;
} argv_copy_t;

/* NULL で終わる tokens を copy に複製して解析し、解析のエラーを橋渡しで diags に移す */
static mnd_args_status_t parse_tokens(const mnd_args_program_t *program, const char *const *tokens,
                                      argv_copy_t *copy, mnd_args_result_t *result, mnd_diag_list_t *diags)
{
    clopts_errors_t errors = CLOPTS_ERRORS_INIT;
    mnd_args_status_t status;

    copy->argc = 0;
    while (tokens[copy->argc] != NULL)
    {
        NAOS_REQUIRE_TRUE(copy->argc < ARGV_MAX_ARGS);
        NAOS_REQUIRE_TRUE(strlen(tokens[copy->argc]) < ARGV_MAX_LEN);
        strcpy(copy->storage[copy->argc], tokens[copy->argc]);
        copy->argv[copy->argc] = copy->storage[copy->argc];
        copy->argc++;
    }
    copy->argv[copy->argc] = NULL;

    status = mnd_args_parse(program, copy->argc, copy->argv, result, &errors);
    mnd_cli_clopts_errors_to_diags(&errors, diags);
    clopts_errors_free(&errors);

    return status;
}

/*
 * プログラムの宣言: 名前、実行時に渡したバージョン、説明と 4 つの子コマンドを持つルートコマンド（1.1、13.1）。
 * ルートコマンドはオプションと位置引数を持たず、子コマンドは末端コマンドで、引き継ぐオプションを使わない
 */
NAOS_TEST(run_commands_program_declares_four_subcommands)
{
    static const char version[] = "9.8.7-commands";
    mnd_args_program_t program = mnd_cli_program(version);
    const mnd_args_command_t *root = program.root;
    size_t i;
    size_t j;

    NAOS_CHECK_STR_EQ("monde", program.name);
    NAOS_CHECK_TRUE(program.version == version);
    NAOS_REQUIRE_NOT_NULL(root);
    NAOS_CHECK_STR_EQ("Translate Markdown documents through gettext .pot/.po files.", root->description);
    NAOS_CHECK_UINT_EQ(0, root->option_count);
    NAOS_CHECK_UINT_EQ(0, root->positional_count);
    NAOS_REQUIRE_UINT_EQ(EXPECTED_COMMAND_COUNT, root->command_count);
    for (i = 0; i < EXPECTED_COMMAND_COUNT; i++)
    {
        const mnd_args_command_t *command = &root->commands[i];

        NAOS_TRACE_PUSH("%s", expected_commands[i].name);
        NAOS_CHECK_STR_EQ(expected_commands[i].name, command->name);
        NAOS_CHECK_TRUE((command->summary != NULL) && (command->summary[0] != '\0'));
        NAOS_CHECK_UINT_EQ(0, command->command_count);
        for (j = 0; j < command->option_count; j++)
        {
            NAOS_CHECK_FALSE(command->options[j].inherited);
        }
        NAOS_TRACE_POP();
    }
}

/* バージョンは呼び出しごとに差し替わり、宣言のテーブルは共有される */
NAOS_TEST(run_commands_program_version_replaced_per_call)
{
    mnd_args_program_t a = mnd_cli_program("1.0.0");
    mnd_args_program_t b = mnd_cli_program("2.0.0");
    char *out;

    NAOS_CHECK_STR_EQ("1.0.0", a.version);
    NAOS_CHECK_STR_EQ("2.0.0", b.version);
    NAOS_CHECK_TRUE(a.root == b.root);
    out = mnd_args_format_version(&b);
    NAOS_REQUIRE_NOT_NULL(out);
    NAOS_CHECK_STR_EQ("monde 2.0.0\n", out);
    mnd_args_text_free(out);
}

/* 各サブコマンドの位置引数とオプションが設計の表のとおりで、出力と言語は必須でない（2.1、2.2、5.2、5.3） */
NAOS_TEST(run_commands_declarations_match_design_table)
{
    mnd_args_program_t program = mnd_cli_program(TEST_CLI_VERSION);
    size_t i;
    size_t j;

    for (i = 0; i < EXPECTED_COMMAND_COUNT; i++)
    {
        const expected_command_t *expected = &expected_commands[i];
        const mnd_args_command_t *command = find_command(&program, expected->name);

        NAOS_TRACE_PUSH("%s", expected->name);
        NAOS_REQUIRE_NOT_NULL(command);
        NAOS_CHECK_UINT_EQ(expected->positional_count, command->positional_count);
        NAOS_CHECK_UINT_EQ(expected->positional_count, command->min_positionals);
        NAOS_CHECK_UINT_EQ(expected->positional_count, command->max_positionals);
        for (j = 0; (j < expected->positional_count) && (j < command->positional_count); j++)
        {
            NAOS_CHECK_STR_EQ(expected->positionals[j], command->positionals[j].name);
            NAOS_CHECK_NOT_NULL(command->positionals[j].help);
        }

        NAOS_CHECK_UINT_EQ(expected->option_count, command->option_count);
        for (j = 0; j < expected->option_count; j++)
        {
            const mnd_args_option_t *option = find_option(command, expected->options[j].long_name);

            NAOS_TRACE_PUSH("--%s", expected->options[j].long_name);
            NAOS_REQUIRE_NOT_NULL(option);
            NAOS_CHECK_TRUE(option == &command->options[j]);
            NAOS_CHECK_INT_EQ((int)expected->options[j].short_name, (int)option->short_name);
            NAOS_CHECK_INT_EQ((int)expected->options[j].kind, (int)option->kind);
            NAOS_CHECK_FALSE(option->required);
            NAOS_CHECK_NULL(option->default_value);
            NAOS_CHECK_TRUE((option->help != NULL) && (option->help[0] != '\0'));
            NAOS_TRACE_POP();
        }
        NAOS_TRACE_POP();
    }
}

/* 宣言の誤りなく解析でき、最小の引数で各サブコマンドが選ばれる */
NAOS_TEST(run_commands_minimal_argv_parses_without_decl_error)
{
    static const char *const extract_argv[] = {"monde", "extract", "in.md", NULL};
    static const char *const init_argv[] = {"monde", "init", "in.pot", NULL};
    static const char *const update_argv[] = {"monde", "update", "new.pot", "old.po", NULL};
    static const char *const translate_argv[] = {"monde", "translate", "in.md", "ja.po", NULL};
    static const char *const *const argvs[] = {extract_argv, init_argv, update_argv, translate_argv};
    mnd_args_program_t program = mnd_cli_program(TEST_CLI_VERSION);
    size_t i;

    for (i = 0; i < sizeof argvs / sizeof argvs[0]; i++)
    {
        mnd_args_result_t result = MND_ARGS_RESULT_INIT;
        mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
        argv_copy_t copy;
        mnd_args_status_t status = parse_tokens(&program, argvs[i], &copy, &result, &diags);

        NAOS_TRACE_PUSH("%s", argvs[i][1]);
        NAOS_CHECK_INT_EQ((int)MND_ARGS_OK, (int)status);
        NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
        if (mnd_diag_list_count(&diags) > 0)
        {
            NAOS_NOTE("first diagnostic: %s", mnd_diag_list_at(&diags, 0)->message);
        }
        NAOS_CHECK_UINT_EQ(2, mnd_args_depth(&result));
        NAOS_REQUIRE_NOT_NULL(mnd_args_command(&result));
        NAOS_CHECK_STR_EQ(argvs[i][1], mnd_args_command(&result)->name);
        NAOS_CHECK_FALSE(mnd_args_is_set(&result, MND_CLI_OPT_OUTPUT));
        NAOS_CHECK_NULL(mnd_args_value(&result, MND_CLI_OPT_OUTPUT));
        NAOS_CHECK_FALSE(mnd_args_flag(&result, MND_CLI_OPT_QUIET));
        mnd_args_result_free(&result);
        mnd_diag_list_free(&diags);
        NAOS_TRACE_POP();
    }
}

/*
 * "monde --help"（command_name が NULL のとき）または "monde <command_name> --help" を解析し、
 * そのヘルプの文面を out に追加する
 */
static void format_help_of(const char *command_name, mnd_buf_t *out)
{
    static const char *const global_argv[] = {"monde", "--help", NULL};
    const char *command_argv[] = {"monde", NULL, "--help", NULL};
    argv_copy_t copy;
    mnd_args_program_t program = mnd_cli_program(TEST_CLI_VERSION);
    mnd_args_result_t result = MND_ARGS_RESULT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_args_status_t status;
    char *text;

    command_argv[1] = command_name;
    status = parse_tokens(&program, (command_name != NULL) ? command_argv : global_argv, &copy, &result, &diags);
    NAOS_REQUIRE_INT_EQ((int)MND_ARGS_HELP, (int)status);
    NAOS_REQUIRE_UINT_EQ((command_name != NULL) ? 2 : 1, mnd_args_depth(&result));
    text = mnd_args_format_help(&result);
    NAOS_REQUIRE_NOT_NULL(text);
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(out, text));
    mnd_args_text_free(text);
    mnd_args_result_free(&result);
    mnd_diag_list_free(&diags);
}

/* 各サブコマンドのヘルプに、宣言したすべてのオプションと位置引数が現れる（1.3） */
NAOS_TEST(run_commands_help_lists_all_options_and_positionals)
{
    mnd_args_program_t program = mnd_cli_program(TEST_CLI_VERSION);
    mnd_buf_t global = MND_BUF_INIT;
    size_t i;
    size_t j;

    format_help_of(NULL, &global);
    NAOS_CHECK_TRUE(buf_contains(&global, "Usage: monde "));
    for (i = 0; i < EXPECTED_COMMAND_COUNT; i++)
    {
        const expected_command_t *expected = &expected_commands[i];
        const mnd_args_command_t *command = find_command(&program, expected->name);
        mnd_buf_t help = MND_BUF_INIT;
        mnd_buf_t needle = MND_BUF_INIT;

        NAOS_TRACE_PUSH("%s", expected->name);
        NAOS_REQUIRE_NOT_NULL(command);
        NAOS_CHECK_TRUE(buf_contains(&global, expected->name));
        format_help_of(expected->name, &help);
        for (j = 0; j < expected->positional_count; j++)
        {
            mnd_buf_clear(&needle);
            NAOS_REQUIRE_TRUE(mnd_buf_append_byte(&needle, '<') && mnd_buf_append_str(&needle, expected->positionals[j])
                              && mnd_buf_append_byte(&needle, '>'));
            NAOS_CHECK_TRUE(buf_contains(&help, mnd_buf_cstr(&needle)));
        }
        for (j = 0; j < expected->option_count; j++)
        {
            const expected_option_t *option = &expected->options[j];

            mnd_buf_clear(&needle);
            if (option->short_name != '\0')
            {
                NAOS_REQUIRE_TRUE(mnd_buf_append_byte(&needle, '-')
                                  && mnd_buf_append_byte(&needle, (unsigned char)option->short_name)
                                  && mnd_buf_append_str(&needle, ", "));
            }
            NAOS_REQUIRE_TRUE(mnd_buf_append_str(&needle, "--") && mnd_buf_append_str(&needle, option->long_name));
            NAOS_TRACE_PUSH("%s", mnd_buf_cstr(&needle));
            NAOS_CHECK_TRUE(buf_contains(&help, mnd_buf_cstr(&needle)));
            NAOS_TRACE_POP();
        }
        mnd_buf_free(&needle);
        mnd_buf_free(&help);
        NAOS_TRACE_POP();
    }
    mnd_buf_free(&global);
}

/* 2 つの解析結果が同じサブコマンド・位置引数・オプションの値を持つことを確かめる */
static void check_same_result(const mnd_args_result_t *a, const mnd_args_result_t *b)
{
    size_t i;
    size_t k;

    NAOS_REQUIRE_INT_EQ((int)MND_ARGS_OK, (int)a->status);
    NAOS_REQUIRE_INT_EQ((int)MND_ARGS_OK, (int)b->status);
    NAOS_REQUIRE_TRUE(mnd_args_command(a) == mnd_args_command(b));
    NAOS_REQUIRE_UINT_EQ(mnd_args_positional_count(a), mnd_args_positional_count(b));
    for (i = 0; i < mnd_args_positional_count(a); i++)
    {
        NAOS_CHECK_STR_EQ(mnd_args_positional_at(a, i), mnd_args_positional_at(b, i));
    }
    for (i = 0; i < mnd_args_command(a)->option_count; i++)
    {
        const mnd_args_option_t *option = &mnd_args_command(a)->options[i];
        const char *name = option->long_name;

        NAOS_TRACE_PUSH("--%s", name);
        NAOS_CHECK_TRUE(mnd_args_is_set(a, name) == mnd_args_is_set(b, name));
        if (option->kind == MND_ARGS_FLAG)
        {
            NAOS_CHECK_TRUE(mnd_args_flag(a, name) == mnd_args_flag(b, name));
        }
        else if (option->kind == MND_ARGS_VALUE)
        {
            if ((mnd_args_value(a, name) == NULL) || (mnd_args_value(b, name) == NULL))
            {
                NAOS_CHECK_TRUE(mnd_args_value(a, name) == mnd_args_value(b, name));
            }
            else
            {
                NAOS_CHECK_STR_EQ(mnd_args_value(a, name), mnd_args_value(b, name));
            }
        }
        else
        {
            NAOS_REQUIRE_UINT_EQ(mnd_args_list_count(a, name), mnd_args_list_count(b, name));
            for (k = 0; k < mnd_args_list_count(a, name); k++)
            {
                NAOS_CHECK_STR_EQ(mnd_args_list_at(a, name, k), mnd_args_list_at(b, name, k));
            }
        }
        NAOS_TRACE_POP();
    }
}

/* 宣言したすべてのオプションが明示的に指定されている */
static void check_all_set(const mnd_args_result_t *result)
{
    const mnd_args_command_t *command = mnd_args_command(result);
    size_t i;

    for (i = 0; i < command->option_count; i++)
    {
        NAOS_TRACE_PUSH("--%s", command->options[i].long_name);
        NAOS_CHECK_TRUE(mnd_args_is_set(result, command->options[i].long_name));
        NAOS_TRACE_POP();
    }
}

static void check_order_independent(const char *const *argv_a, const char *const *argv_b)
{
    static argv_copy_t copy_a;
    static argv_copy_t copy_b;
    mnd_args_program_t program = mnd_cli_program(TEST_CLI_VERSION);
    mnd_args_result_t a = MND_ARGS_RESULT_INIT;
    mnd_args_result_t b = MND_ARGS_RESULT_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_TRACE_PUSH("%s", argv_a[1]);
    NAOS_CHECK_INT_EQ((int)MND_ARGS_OK, (int)parse_tokens(&program, argv_a, &copy_a, &a, &diags));
    NAOS_CHECK_INT_EQ((int)MND_ARGS_OK, (int)parse_tokens(&program, argv_b, &copy_b, &b, &diags));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    if ((a.status == MND_ARGS_OK) && (b.status == MND_ARGS_OK))
    {
        NAOS_CHECK_STR_EQ(argv_a[1], mnd_args_command(&a)->name);
        check_all_set(&a);
        check_same_result(&a, &b);
    }
    mnd_args_result_free(&a);
    mnd_args_result_free(&b);
    mnd_diag_list_free(&diags);
    NAOS_TRACE_POP();
}

/* オプションと位置引数の順序を入れ替えても同じ解析結果になる（1.7） */
NAOS_TEST(run_commands_order_of_arguments_does_not_matter)
{
    static const char *const extract_a[] = {"monde", "extract", "in.md", "-o", "out.pot", "-q",
                                      "--front-matter-keys", "title,tags", "--project-id-version", "Doc 1.0",
                                      "--msgid-bugs-address", "bugs@example.com", "--no-wrap", NULL};
    static const char *const extract_b[] = {"monde", "extract", "--no-wrap", "--msgid-bugs-address=bugs@example.com",
                                      "--front-matter-keys", "title", "-q", "in.md", "--front-matter-keys", "tags",
                                      "--project-id-version", "Doc 1.0", "--output", "out.pot", NULL};
    static const char *const init_a[] = {"monde", "init", "in.pot", "-o", "ja.po", "-q", "-l", "ja", "--no-wrap", NULL};
    static const char *const init_b[] = {"monde", "init", "--no-wrap", "--language", "ja", "--quiet", "--output=ja.po",
                                   "in.pot", NULL};
    static const char *const update_a[] = {"monde", "update", "new.pot", "old.po", "-o", "out.po", "-q",
                                     "--no-fuzzy-matching", "--no-wrap", NULL};
    static const char *const update_b[] = {"monde", "update", "--no-wrap", "new.pot", "--no-fuzzy-matching", "-q",
                                     "-o", "out.po", "old.po", NULL};
    static const char *const translate_a[] = {"monde", "translate", "in.md", "ja.po", "-o", "out.md", "-q",
                                        "--front-matter-keys", "title", "--use-fuzzy", NULL};
    static const char *const translate_b[] = {"monde", "translate", "--use-fuzzy", "in.md", "--front-matter-keys=title",
                                        "--quiet", "ja.po", "-o", "out.md", NULL};

    check_order_independent(extract_a, extract_b);
    check_order_independent(init_a, init_b);
    check_order_independent(update_a, update_b);
    check_order_independent(translate_a, translate_b);
}

/* 4 つのサブコマンド名はそれぞれのハンドラを引き、未知の名前は NULL を返す */
NAOS_TEST(run_commands_find_handler_maps_names)
{
    NAOS_CHECK_TRUE(mnd_cli_find_handler("extract") == mnd_cli_cmd_extract);
    NAOS_CHECK_TRUE(mnd_cli_find_handler("init") == mnd_cli_cmd_init);
    NAOS_CHECK_TRUE(mnd_cli_find_handler("update") == mnd_cli_cmd_update);
    NAOS_CHECK_TRUE(mnd_cli_find_handler("translate") == mnd_cli_cmd_translate);
    NAOS_CHECK_TRUE(mnd_cli_find_handler("help") == NULL);
    NAOS_CHECK_TRUE(mnd_cli_find_handler("Extract") == NULL);
    NAOS_CHECK_TRUE(mnd_cli_find_handler("extractx") == NULL);
    NAOS_CHECK_TRUE(mnd_cli_find_handler("") == NULL);
    NAOS_CHECK_TRUE(mnd_cli_find_handler(NULL) == NULL);
}

/* 宣言したすべてのサブコマンドにハンドラがある */
NAOS_TEST(run_commands_every_declared_command_has_handler)
{
    mnd_args_program_t program = mnd_cli_program(TEST_CLI_VERSION);
    size_t i;

    NAOS_REQUIRE_UINT_EQ(EXPECTED_COMMAND_COUNT, program.root->command_count);
    for (i = 0; i < program.root->command_count; i++)
    {
        NAOS_TRACE_PUSH("%s", program.root->commands[i].name);
        NAOS_CHECK_TRUE(mnd_cli_find_handler(program.root->commands[i].name) != NULL);
        NAOS_TRACE_POP();
    }
}

/* ---- Run: 入口の分岐と終了コード（3.1） ---- */

/* NULL で終わる tokens を copy に複製する（argv[argc] == NULL） */
static void copy_tokens(const char *const *tokens, argv_copy_t *copy)
{
    copy->argc = 0;
    while (tokens[copy->argc] != NULL)
    {
        NAOS_REQUIRE_TRUE(copy->argc < ARGV_MAX_ARGS);
        NAOS_REQUIRE_TRUE(strlen(tokens[copy->argc]) < ARGV_MAX_LEN);
        strcpy(copy->storage[copy->argc], tokens[copy->argc]);
        copy->argv[copy->argc] = copy->storage[copy->argc];
        copy->argc++;
    }
    copy->argv[copy->argc] = NULL;
}

/* test_cli_run で tokens を実行する（name はテストごとに一意） */
static void run_tokens(const char *name, const char *const *tokens, test_cli_result_t *result)
{
    static argv_copy_t copy;

    copy_tokens(tokens, &copy);
    NAOS_REQUIRE_TRUE(test_cli_run(name, NULL, 0, copy.argc, copy.argv, result));
}

/* 作業ディレクトリの <name><suffix> を mode で開く（テストコードのみ fopen を直接使う） */
static FILE *open_work_stream(const char *name, const char *suffix, const char *mode)
{
    mnd_buf_t path = MND_BUF_INIT;
    FILE *fp = NULL;

    if (test_work_path(name, &path) && mnd_buf_append_str(&path, suffix))
    {
        fp = fopen(mnd_buf_cstr(&path), mode);
    }
    mnd_buf_free(&path);

    return fp;
}

/* test_cli_run と同じ環境で、ハンドラを find_handler で引く mnd_cli_run_with を呼ぶ */
static void run_tokens_with(const char *name, const char *const *tokens, mnd_cli_find_handler_fn find_handler,
                            test_cli_result_t *result)
{
    static argv_copy_t copy;
    mnd_buf_t out_name = MND_BUF_INIT;
    mnd_buf_t err_name = MND_BUF_INIT;
    mnd_cli_env_t env;
    FILE *in;
    FILE *out;
    FILE *err;

    copy_tokens(tokens, &copy);
    NAOS_REQUIRE_TRUE(test_write_file(name, "", 0));
    in = open_work_stream(name, "", "rb");
    out = open_work_stream(name, ".out", "wb");
    err = open_work_stream(name, ".err", "wb");
    NAOS_REQUIRE_TRUE((in != NULL) && (out != NULL) && (err != NULL));

    env.in = in;
    env.out = out;
    env.err = err;
    env.version = TEST_CLI_VERSION;
    env.getenv = test_env_getenv;
    env.now = test_clock_now;
    result->exit_code = mnd_cli_run_with(&env, copy.argc, copy.argv, find_handler);

    NAOS_REQUIRE_TRUE((fclose(in) == 0) && (fclose(out) == 0) && (fclose(err) == 0));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&out_name, name) && mnd_buf_append_str(&out_name, ".out"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&err_name, name) && mnd_buf_append_str(&err_name, ".err"));
    NAOS_REQUIRE_TRUE(test_read_file(mnd_buf_cstr(&out_name), &result->out));
    NAOS_REQUIRE_TRUE(test_read_file(mnd_buf_cstr(&err_name), &result->err));
    mnd_buf_free(&out_name);
    mnd_buf_free(&err_name);
}

/* 期待するヘルプの文面（command_name が NULL なら全体） */
static void expected_help(const char *command_name, mnd_buf_t *out)
{
    format_help_of(command_name, out);
}

/* --help / -h / help は全体のヘルプを out に書いて 0 を返し、err には何も書かない（1.2） */
NAOS_TEST(run_entry_global_help_goes_to_out)
{
    static const char *const long_argv[] = {"monde", "--help", NULL};
    static const char *const short_argv[] = {"monde", "-h", NULL};
    static const char *const help_argv[] = {"monde", "help", NULL};
    static const char *const *const argvs[] = {long_argv, short_argv, help_argv};
    static const char *const names[] = {"run_entry_global_help_long", "run_entry_global_help_short",
                                        "run_entry_global_help_word"};
    mnd_buf_t expected = MND_BUF_INIT;
    size_t i;

    expected_help(NULL, &expected);
    NAOS_CHECK_TRUE(buf_contains(&expected, "extract"));
    for (i = 0; i < sizeof argvs / sizeof argvs[0]; i++)
    {
        test_cli_result_t result = TEST_CLI_RESULT_INIT;

        NAOS_TRACE_PUSH("%s", argvs[i][1]);
        run_tokens(names[i], argvs[i], &result);
        NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
        NAOS_CHECK_MEM_EQ(expected.data, expected.len, result.out.data, result.out.len);
        NAOS_CHECK_UINT_EQ(0, result.err.len);
        test_cli_result_free(&result);
        NAOS_TRACE_POP();
    }
    mnd_buf_free(&expected);
}

/* <sub> --help / <sub> -h / help <sub> はそのサブコマンドのヘルプを out に書いて 0 を返す（1.3） */
NAOS_TEST(run_entry_subcommand_help_goes_to_out)
{
    static const char *const extract_argv[] = {"monde", "extract", "--help", NULL};
    static const char *const init_argv[] = {"monde", "init", "-h", NULL};
    static const char *const update_argv[] = {"monde", "help", "update", NULL};
    static const char *const translate_argv[] = {"monde", "translate", "in.md", "--help", NULL};
    static const char *const *const argvs[] = {extract_argv, init_argv, update_argv, translate_argv};
    static const char *const commands[] = {"extract", "init", "update", "translate"};
    static const char *const names[] = {"run_entry_sub_help_extract", "run_entry_sub_help_init",
                                        "run_entry_sub_help_update", "run_entry_sub_help_translate"};
    size_t i;

    for (i = 0; i < sizeof argvs / sizeof argvs[0]; i++)
    {
        test_cli_result_t result = TEST_CLI_RESULT_INIT;
        mnd_buf_t expected = MND_BUF_INIT;

        NAOS_TRACE_PUSH("%s", commands[i]);
        expected_help(commands[i], &expected);
        run_tokens(names[i], argvs[i], &result);
        NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
        NAOS_CHECK_MEM_EQ(expected.data, expected.len, result.out.data, result.out.len);
        NAOS_CHECK_UINT_EQ(0, result.err.len);
        mnd_buf_free(&expected);
        test_cli_result_free(&result);
        NAOS_TRACE_POP();
    }
}

/* --version は "monde <version>" の 1 行を out に書いて 0 を返す（1.4） */
NAOS_TEST(run_entry_version_goes_to_out)
{
    static const char *const tokens[] = {"monde", "--version", NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_tokens("run_entry_version", tokens, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_STR_EQ("monde " TEST_CLI_VERSION "\n", mnd_buf_cstr(&result.out));
    NAOS_CHECK_UINT_EQ(0, result.err.len);
    test_cli_result_free(&result);
}

/* 引数なしは全体のヘルプを err に書いて 2 を返し、out には何も書かない（1.5、9.7） */
NAOS_TEST(run_entry_no_arguments_prints_help_to_err)
{
    static const char *const tokens[] = {"monde", NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    expected_help(NULL, &expected);
    run_tokens("run_entry_no_arguments", tokens, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_MEM_EQ(expected.data, expected.len, result.err.data, result.err.len);
    mnd_buf_free(&expected);
    test_cli_result_free(&result);
}

/* 使い方のエラーの 1 件: 引数列、err の期待する全体 */
typedef struct usage_case
{
    const char *name;
    const char *tokens[8];
    const char *expected_err;
} usage_case_t;

/* 引数の誤りは "monde: error: <message>" と案内の 1 行を err に書いて 2 を返す（1.6、9.7、9.8、9.9） */
NAOS_TEST(run_entry_usage_errors_print_message_and_hint)
{
    static const usage_case_t cases[] = {
        {"run_entry_usage_unknown_command", {"monde", "frobnicate", NULL},
         "monde: error: unknown command 'frobnicate'\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_usage_unknown_option", {"monde", "extract", "in.md", "--bogus", NULL},
         "monde: error: unknown option '--bogus'\n"
         "Run 'monde extract --help' for usage.\n"},
        {"run_entry_usage_too_many", {"monde", "init", "a.pot", "b.pot", NULL},
         "monde: error: unexpected argument 'b.pot'\n"
         "Run 'monde init --help' for usage.\n"},
        {"run_entry_usage_too_few", {"monde", "update", "new.pot", NULL},
         "monde: error: missing argument <PO>\n"
         "Run 'monde update --help' for usage.\n"},
        {"run_entry_usage_empty_item", {"monde", "translate", "in.md", "ja.po", "--front-matter-keys", "title,,tags",
                                        NULL},
         "monde: error: option '--front-matter-keys' has an empty item in 'title,,tags'\n"
         "Run 'monde translate --help' for usage.\n"},
    };
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        test_cli_result_t result = TEST_CLI_RESULT_INIT;

        NAOS_TRACE_PUSH("%s", cases[i].name);
        run_tokens(cases[i].name, cases[i].tokens, &result);
        NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
        NAOS_CHECK_UINT_EQ(0, result.out.len);
        NAOS_CHECK_STR_EQ(cases[i].expected_err, mnd_buf_cstr(&result.err));
        test_cli_result_free(&result);
        NAOS_TRACE_POP();
    }
}

/*
 * 子コマンドの選択前の誤り（未知のサブコマンド、未知のオプション）は、後ろに --help があっても
 * 改訂前と同じく使い方のエラーとして 2 を返し、ヘルプを出さない（8.10、13.4）
 */
NAOS_TEST(run_entry_unknown_command_with_help_stays_usage_error)
{
    static const usage_case_t cases[] = {
        {"run_entry_bogus_command_help", {"monde", "bogus", "--help", NULL},
         "monde: error: unknown command 'bogus'\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_bogus_command_short_help", {"monde", "bogus", "-h", NULL},
         "monde: error: unknown command 'bogus'\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_bogus_option_help", {"monde", "--bogus", "--help", NULL},
         "monde: error: unknown option '--bogus'\n"
         "Run 'monde --help' for usage.\n"},
    };
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        test_cli_result_t result = TEST_CLI_RESULT_INIT;

        NAOS_TRACE_PUSH("%s", cases[i].name);
        run_tokens(cases[i].name, cases[i].tokens, &result);
        NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
        NAOS_CHECK_UINT_EQ(0, result.out.len);
        NAOS_CHECK_STR_EQ(cases[i].expected_err, mnd_buf_cstr(&result.err));
        test_cli_result_free(&result);
        NAOS_TRACE_POP();
    }
}

/*
 * 設計書「改訂前の 1 段の構成との違い」の表のうち、使い方のエラーのまま文面が変わるもの（13.4）。
 * 子コマンドの選択前にも、オプションの形の規則と短いオプションを 1 文字ずつ処理する規則を適用する
 */
NAOS_TEST(run_entry_revised_usage_error_messages)
{
    static const usage_case_t cases[] = {
        {"run_entry_revised_help_word_two", {"monde", "help", "a", "b", NULL},
         "monde: error: unknown command 'a'\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_revised_help_word_extra", {"monde", "help", "extract", "x", NULL},
         "monde: error: unexpected argument 'x'\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_revised_help_word_help", {"monde", "help", "extract", "--help", NULL},
         "monde: error: unexpected argument '--help'\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_revised_unknown_long_value", {"monde", "--foo=bar", NULL},
         "monde: error: unknown option '--foo'\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_revised_version_value", {"monde", "--version=1", NULL},
         "monde: error: option '--version' does not take a value\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_revised_help_value", {"monde", "--help=1", NULL},
         "monde: error: option '--help' does not take a value\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_revised_short_cluster", {"monde", "-xy", NULL},
         "monde: error: unknown option '-x'\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_revised_short_cluster_v", {"monde", "-vh", NULL},
         "monde: error: unknown option '-v'\n"
         "Run 'monde --help' for usage.\n"},
        {"run_entry_revised_terminator_command", {"monde", "--", "extract", NULL},
         "monde: error: unexpected argument 'extract'\n"
         "Run 'monde --help' for usage.\n"},
    };
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        test_cli_result_t result = TEST_CLI_RESULT_INIT;

        NAOS_TRACE_PUSH("%s", cases[i].name);
        run_tokens(cases[i].name, cases[i].tokens, &result);
        NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
        NAOS_CHECK_UINT_EQ(0, result.out.len);
        NAOS_CHECK_STR_EQ(cases[i].expected_err, mnd_buf_cstr(&result.err));
        test_cli_result_free(&result);
        NAOS_TRACE_POP();
    }
}

/* "monde --" は子コマンドの未指定として、全体のヘルプを err に書いて 2 を返す（2.5、13.4） */
NAOS_TEST(run_entry_terminator_alone_prints_help_to_err)
{
    static const char *const tokens[] = {"monde", "--", NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;
    mnd_buf_t expected = MND_BUF_INIT;

    expected_help(NULL, &expected);
    run_tokens("run_entry_terminator_alone", tokens, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_MEM_EQ(expected.data, expected.len, result.err.data, result.err.len);
    mnd_buf_free(&expected);
    test_cli_result_free(&result);
}

/*
 * 子コマンドの選択前の、h で始まる短いオプションのまとめ書きは、全体のヘルプを out に書いて 0 を返す
 * （4.3、4.5、8.1、13.4 の例外。monde extract -hx と同じ扱い）
 */
NAOS_TEST(run_entry_short_help_cluster_prints_help)
{
    static const char *const tokens_hx[] = {"monde", "-hx", NULL};
    static const char *const tokens_hh[] = {"monde", "-hh", NULL};
    static const char *const tokens_h1[] = {"monde", "-h1", NULL};
    static const char *const tokens_heq[] = {"monde", "-h=1", NULL};
    static const char *const *const cases[] = {tokens_hx, tokens_hh, tokens_h1, tokens_heq};
    mnd_buf_t expected = MND_BUF_INIT;
    size_t i;

    expected_help(NULL, &expected);
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        test_cli_result_t result = TEST_CLI_RESULT_INIT;

        NAOS_TRACE_PUSH("%s", cases[i][1]);
        run_tokens("run_entry_short_help_cluster", cases[i], &result);
        NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
        NAOS_CHECK_UINT_EQ(0, result.err.len);
        NAOS_CHECK_MEM_EQ(expected.data, expected.len, result.out.data, result.out.len);
        test_cli_result_free(&result);
        NAOS_TRACE_POP();
    }
    mnd_buf_free(&expected);
}

/* 入口から呼ばれたハンドラ（仮の実装を含む）の失敗は、err に診断を書いて 1 を返し、out には何も書かない（9.6、9.8） */
NAOS_TEST(run_entry_handler_failure_reports_diags_and_returns_1)
{
    static const char *const tokens[] = {"monde", "extract", "run_entry_handler_failure_missing.md", NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    NAOS_REQUIRE_TRUE(test_remove_file("run_entry_handler_failure_missing.md"));
    run_tokens("run_entry_handler_failure", tokens, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_TRUE(buf_contains(&result.err, "error: "));
    NAOS_CHECK_FALSE(buf_contains(&result.err, "for usage."));
    test_cli_result_free(&result);
}

/* ---- 差し替えのハンドラ ---- */

/* 差し替えのハンドラが見た文脈 */
static struct
{
    int calls;
    bool has_env;
    const char *version; /* env は呼び出し側の自動変数なので、ハンドラの中で値を写す */
    const char *command;
    size_t positionals;
    bool quiet;
    bool usage_error_initial;
} g_seen;

static void record_ctx(const mnd_cli_ctx_t *ctx)
{
    g_seen.calls++;
    g_seen.has_env = (ctx->env != NULL);
    g_seen.version = (ctx->env != NULL) ? ctx->env->version : NULL;
    g_seen.command = (mnd_args_command(ctx->result) != NULL) ? mnd_args_command(ctx->result)->name : NULL;
    g_seen.positionals = mnd_args_positional_count(ctx->result);
    g_seen.quiet = ctx->quiet;
    g_seen.usage_error_initial = ctx->usage_error;
}

static void reset_seen(void)
{
    memset(&g_seen, 0, sizeof g_seen);
}

/* 警告と 3 形式のエラーを順に加えて失敗する */
static bool handler_fail_with_diags(mnd_cli_ctx_t *ctx)
{
    record_ctx(ctx);
    mnd_diag_add(ctx->diags, MND_SEVERITY_WARNING, "a.md", 3, "first warning");
    mnd_diag_add(ctx->diags, MND_SEVERITY_ERROR, "b.po", 0, "second error");
    mnd_diag_add(ctx->diags, MND_SEVERITY_ERROR, NULL, 0, "third error");
    return false;
}

/* 使い方のエラーの印を付けて失敗する */
static bool handler_fail_usage(mnd_cli_ctx_t *ctx)
{
    record_ctx(ctx);
    mnd_diag_add(ctx->diags, MND_SEVERITY_ERROR, NULL, 0, "cannot determine the target language");
    ctx->usage_error = true;
    return false;
}

/* 警告を 1 件加えて成功する */
static bool handler_succeed_with_warning(mnd_cli_ctx_t *ctx)
{
    record_ctx(ctx);
    mnd_diag_add(ctx->diags, MND_SEVERITY_WARNING, "c.md", 7, "just a warning");
    return true;
}

/* 成功を返すが、エラーの診断を加えている */
static bool handler_succeed_with_error(mnd_cli_ctx_t *ctx)
{
    record_ctx(ctx);
    mnd_diag_add(ctx->diags, MND_SEVERITY_ERROR, NULL, 0, "inconsistent error");
    return true;
}

static mnd_cli_handler_fn find_fail_with_diags(const char *name)
{
    (void)name;
    return handler_fail_with_diags;
}

static mnd_cli_handler_fn find_fail_usage(const char *name)
{
    (void)name;
    return handler_fail_usage;
}

static mnd_cli_handler_fn find_succeed_with_warning(const char *name)
{
    (void)name;
    return handler_succeed_with_warning;
}

static mnd_cli_handler_fn find_succeed_with_error(const char *name)
{
    (void)name;
    return handler_succeed_with_error;
}

static mnd_cli_handler_fn find_none(const char *name)
{
    (void)name;
    return NULL;
}

/* ハンドラの失敗では、集まった診断を順にすべて err に書いて 1 を返す（9.1〜9.4、9.6、9.8） */
NAOS_TEST(run_entry_failed_handler_prints_all_diags_in_order)
{
    static const char *const tokens[] = {"monde", "update", "new.pot", "old.po", NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    reset_seen();
    run_tokens_with("run_entry_failed_handler_diags", tokens, find_fail_with_diags, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_INT_EQ(1, g_seen.calls);
    NAOS_CHECK_STR_EQ("update", g_seen.command);
    NAOS_CHECK_UINT_EQ(2, g_seen.positionals);
    NAOS_CHECK_FALSE(g_seen.quiet);
    NAOS_CHECK_FALSE(g_seen.usage_error_initial);
    NAOS_CHECK_TRUE(g_seen.has_env);
    NAOS_CHECK_STR_EQ(TEST_CLI_VERSION, g_seen.version);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ("a.md:3: warning: first warning\n"
                      "b.po: error: second error\n"
                      "monde: error: third error\n",
                      mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
}

/* 使い方のエラーの印を付けて失敗したハンドラでは 2 を返す（9.7） */
NAOS_TEST(run_entry_handler_usage_error_returns_2)
{
    static const char *const tokens[] = {"monde", "init", "in.pot", NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    reset_seen();
    run_tokens_with("run_entry_handler_usage_error", tokens, find_fail_usage, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
    NAOS_CHECK_INT_EQ(1, g_seen.calls);
    NAOS_CHECK_STR_EQ("init", g_seen.command);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ("monde: error: cannot determine the target language\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
}

/* 警告だけで成功したハンドラは 0 を返し、警告は err に出る。-q は文脈の quiet になる（9.5、8.x） */
NAOS_TEST(run_entry_successful_handler_returns_0_and_passes_quiet)
{
    static const char *const tokens[] = {"monde", "translate", "-q", "in.md", "ja.po", NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    reset_seen();
    run_tokens_with("run_entry_successful_handler", tokens, find_succeed_with_warning, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    NAOS_CHECK_INT_EQ(1, g_seen.calls);
    NAOS_CHECK_STR_EQ("translate", g_seen.command);
    NAOS_CHECK_TRUE(g_seen.quiet);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ("c.md:7: warning: just a warning\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
}

/* 成功を返してもエラーの診断があれば成功として扱わず 1 を返す（9.6） */
NAOS_TEST(run_entry_success_with_error_diag_returns_1)
{
    static const char *const tokens[] = {"monde", "extract", "--quiet", "in.md", NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    reset_seen();
    run_tokens_with("run_entry_success_with_error", tokens, find_succeed_with_error, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_TRUE(g_seen.quiet);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_STR_EQ("monde: error: inconsistent error\n", mnd_buf_cstr(&result.err));
    test_cli_result_free(&result);
}

/* ハンドラが見つからないのは内部エラーで、診断を err に書いて 1 を返す */
NAOS_TEST(run_entry_missing_handler_is_internal_error)
{
    static const char *const tokens[] = {"monde", "extract", "in.md", NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    run_tokens_with("run_entry_missing_handler", tokens, find_none, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_TRUE(buf_contains(&result.err, "monde: error: "));
    test_cli_result_free(&result);
}

/* ヘルプ・バージョン・使い方のエラーではハンドラを呼ばない */
NAOS_TEST(run_entry_handler_not_called_for_help_version_usage)
{
    static const char *const help_argv[] = {"monde", "extract", "--help", NULL};
    static const char *const version_argv[] = {"monde", "--version", NULL};
    static const char *const usage_argv[] = {"monde", "extract", NULL};
    static const char *const none_argv[] = {"monde", NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    reset_seen();
    run_tokens_with("run_entry_not_called_help", help_argv, find_fail_with_diags, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    run_tokens_with("run_entry_not_called_version", version_argv, find_fail_with_diags, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_OK, result.exit_code);
    run_tokens_with("run_entry_not_called_usage", usage_argv, find_fail_with_diags, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
    run_tokens_with("run_entry_not_called_none", none_argv, find_fail_with_diags, &result);
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_USAGE, result.exit_code);
    NAOS_CHECK_INT_EQ(0, g_seen.calls);
    test_cli_result_free(&result);
}

NAOS_SUITE(run,
    NAOS_ADD_TEST(run_scaffold_exit_codes_and_work_dir),
    NAOS_ADD_TEST(run_util_write_and_read_back_file),
    NAOS_ADD_TEST(run_util_getenv_looks_up_table),
    NAOS_ADD_TEST(run_util_clock_returns_fixed_time),
    NAOS_ADD_TEST(run_commands_program_declares_four_subcommands),
    NAOS_ADD_TEST(run_commands_program_version_replaced_per_call),
    NAOS_ADD_TEST(run_commands_declarations_match_design_table),
    NAOS_ADD_TEST(run_commands_minimal_argv_parses_without_decl_error),
    NAOS_ADD_TEST(run_commands_help_lists_all_options_and_positionals),
    NAOS_ADD_TEST(run_commands_order_of_arguments_does_not_matter),
    NAOS_ADD_TEST(run_commands_find_handler_maps_names),
    NAOS_ADD_TEST(run_commands_every_declared_command_has_handler),
    NAOS_ADD_TEST(run_entry_global_help_goes_to_out),
    NAOS_ADD_TEST(run_entry_subcommand_help_goes_to_out),
    NAOS_ADD_TEST(run_entry_version_goes_to_out),
    NAOS_ADD_TEST(run_entry_no_arguments_prints_help_to_err),
    NAOS_ADD_TEST(run_entry_usage_errors_print_message_and_hint),
    NAOS_ADD_TEST(run_entry_unknown_command_with_help_stays_usage_error),
    NAOS_ADD_TEST(run_entry_revised_usage_error_messages),
    NAOS_ADD_TEST(run_entry_terminator_alone_prints_help_to_err),
    NAOS_ADD_TEST(run_entry_short_help_cluster_prints_help),
    NAOS_ADD_TEST(run_entry_handler_failure_reports_diags_and_returns_1),
    NAOS_ADD_TEST(run_entry_failed_handler_prints_all_diags_in_order),
    NAOS_ADD_TEST(run_entry_handler_usage_error_returns_2),
    NAOS_ADD_TEST(run_entry_successful_handler_returns_0_and_passes_quiet),
    NAOS_ADD_TEST(run_entry_success_with_error_diag_returns_1),
    NAOS_ADD_TEST(run_entry_missing_handler_is_internal_error),
    NAOS_ADD_TEST(run_entry_handler_not_called_for_help_version_usage)
)
