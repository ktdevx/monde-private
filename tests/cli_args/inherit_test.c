#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "cli_args/args.h"
#include "foundation/diag.h"
#include "test_decl.h"

/* 子孫に引き継ぐオプションと、経路全体にわたる必須の検査（3.6、5.7、7.1、12.1〜12.6） */

#define INHERIT_MAX_ARGS 14
#define INHERIT_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct inherit_run
{
    char storage[INHERIT_MAX_ARGS][INHERIT_MAX_LEN];
    char *argv[INHERIT_MAX_ARGS + 1];
    mnd_args_result_t result;
    mnd_diag_list_t diags;
    mnd_args_status_t status;
} inherit_run_t;

/* tokens の argc 個を引数列として program で解析する */
static void run_parse_with(inherit_run_t *run, const mnd_args_program_t *program, int argc,
                           const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    mnd_args_result_init(&run->result);
    mnd_diag_list_init(&run->diags);

    NAOS_REQUIRE_TRUE(argc <= INHERIT_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < INHERIT_MAX_LEN);
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

/* tokens の argc 個を引数列として test_nest_program で解析する */
static void run_parse(inherit_run_t *run, int argc, const char *const *tokens)
{
    run_parse_with(run, &test_nest_program, argc, tokens);
}

static void run_free(inherit_run_t *run)
{
    mnd_args_result_free(&run->result);
    mnd_diag_list_free(&run->diags);
}

/* 診断なしで成功し、経路の最後が command であることを確かめる */
static void check_ok(const inherit_run_t *run, const mnd_args_command_t *command)
{
    NAOS_CHECK_INT_EQ(MND_ARGS_OK, run->status);
    NAOS_CHECK_PTR_EQ(command, mnd_args_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&run->diags));
}

/* 使い方のエラーで、期待するメッセージの診断がちょうど 1 件だけ追加されたことを確かめる（7.2、7.4） */
static void check_usage_error(const inherit_run_t *run, const char *expected)
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

/*
 * nest remote add n u -t x に --verbose（または -v）を 1 か所加えた解析の結果を確かめる。
 * 値は宣言した段（ルートコマンドの段 0）に記録される（12.3、12.4）
 */
static void check_remote_add_with_verbose(const inherit_run_t *run)
{
    check_ok(run, test_nest_remote_add);
    NAOS_REQUIRE_UINT_EQ(3, mnd_args_depth(&run->result));
    NAOS_CHECK_TRUE(mnd_args_flag(&run->result, "verbose"));
    NAOS_CHECK_TRUE(mnd_args_is_set(&run->result, "verbose"));
    NAOS_CHECK_TRUE(run->result.levels[0].values[0].explicit_set);
    NAOS_CHECK_FALSE(mnd_args_flag(&run->result, "force"));
    NAOS_CHECK_FALSE(mnd_args_flag(&run->result, "dry-run"));
    NAOS_CHECK_STR_EQ("x", mnd_args_value(&run->result, "token"));
    NAOS_CHECK_TRUE(run->result.levels[1].values[0].explicit_set);
    NAOS_CHECK_STR_EQ("nest.cfg", mnd_args_value(&run->result, "config"));
    NAOS_CHECK_UINT_EQ(0, mnd_args_list_count(&run->result, "tags"));
    NAOS_REQUIRE_UINT_EQ(2, mnd_args_positional_count(&run->result));
    NAOS_CHECK_STR_EQ("n", mnd_args_positional_at(&run->result, 0));
    NAOS_CHECK_STR_EQ("u", mnd_args_positional_at(&run->result, 1));
}

/* 引き継ぐフラグは子コマンド名の前後のどこで指定しても同じ結果になる（12.1、12.4） */

NAOS_TEST(inherited_flag_before_remote)
{
    static const char *const tokens[] = {"nest", "-v", "remote", "add", "n", "u", "-t", "x"};
    inherit_run_t run;

    run_parse(&run, 8, tokens);
    check_remote_add_with_verbose(&run);
    run_free(&run);
}

NAOS_TEST(inherited_flag_after_remote)
{
    static const char *const tokens[] = {"nest", "remote", "-v", "add", "n", "u", "-t", "x"};
    inherit_run_t run;

    run_parse(&run, 8, tokens);
    check_remote_add_with_verbose(&run);
    run_free(&run);
}

NAOS_TEST(inherited_flag_after_add)
{
    static const char *const tokens[] = {"nest", "remote", "add", "-v", "n", "u", "-t", "x"};
    inherit_run_t run;

    run_parse(&run, 8, tokens);
    check_remote_add_with_verbose(&run);
    run_free(&run);
}

NAOS_TEST(inherited_long_flag_after_add)
{
    static const char *const tokens[] = {"nest", "remote", "add", "n", "u", "-t", "x", "--verbose"};
    inherit_run_t run;

    run_parse(&run, 8, tokens);
    check_remote_add_with_verbose(&run);
    run_free(&run);
}

/* 前後の両方で指定したフラグは同じオプションの繰り返しで、エラーにならない（5.5、12.3） */
NAOS_TEST(inherited_flag_repeated_across_levels)
{
    static const char *const tokens[] = {"nest", "-v", "remote", "-v", "add", "-v", "n", "u", "-t", "x"};
    inherit_run_t run;

    run_parse(&run, 10, tokens);
    check_remote_add_with_verbose(&run);
    run_free(&run);
}

/* 引き継がないオプションは、宣言したコマンドが選択中の間だけ有効（12.2、3.6、5.7） */

NAOS_TEST(non_inherited_option_after_remote_is_unknown)
{
    static const char *const tokens[] = {"nest", "remote", "--dry-run", "add", "n", "u", "-t", "x"};
    inherit_run_t run;

    run_parse(&run, 8, tokens);
    check_usage_error(&run, "nest remote: unknown option '--dry-run'");
    NAOS_CHECK_PTR_EQ(test_nest_remote, mnd_args_command(&run.result));
    run_free(&run);
}

NAOS_TEST(non_inherited_option_after_add_is_unknown)
{
    static const char *const tokens[] = {"nest", "remote", "add", "n", "u", "-t", "x", "--dry-run"};
    inherit_run_t run;

    run_parse(&run, 8, tokens);
    check_usage_error(&run, "nest remote add: unknown option '--dry-run'");
    NAOS_CHECK_PTR_EQ(test_nest_remote_add, mnd_args_command(&run.result));
    run_free(&run);
}

NAOS_TEST(non_inherited_option_before_child_is_recorded)
{
    static const char *const tokens[] = {"nest", "--dry-run", "remote", "add", "n", "u", "-t", "x"};
    inherit_run_t run;

    run_parse(&run, 8, tokens);
    check_ok(&run, test_nest_remote_add);
    NAOS_CHECK_TRUE(mnd_args_flag(&run.result, "dry-run"));
    NAOS_CHECK_TRUE(mnd_args_is_set(&run.result, "dry-run"));
    NAOS_CHECK_FALSE(mnd_args_flag(&run.result, "verbose"));
    run_free(&run);
}

/* remote の引き継ぐオプションは、remote の子孫でないコマンドでは有効でない（12.1） */

NAOS_TEST(remote_option_on_status_is_unknown)
{
    static const char *const tokens[] = {"nest", "status", "--tags", "a"};
    inherit_run_t run;

    run_parse(&run, 4, tokens);
    check_usage_error(&run, "nest status: unknown option '--tags'");
    NAOS_CHECK_PTR_EQ(test_nest_status, mnd_args_command(&run.result));
    run_free(&run);
}

NAOS_TEST(remote_option_before_remote_is_unknown)
{
    static const char *const tokens[] = {"nest", "-t", "x", "remote", "add", "n", "u"};
    inherit_run_t run;

    run_parse(&run, 7, tokens);
    check_usage_error(&run, "nest: unknown option '-t'");
    run_free(&run);
}

NAOS_TEST(root_inherited_option_on_status_is_recorded)
{
    static const char *const tokens[] = {"nest", "status", "-v", "--config=s.cfg"};
    inherit_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run, test_nest_status);
    NAOS_CHECK_TRUE(mnd_args_flag(&run.result, "verbose"));
    NAOS_CHECK_STR_EQ("s.cfg", mnd_args_value(&run.result, "config"));
    run_free(&run);
}

/* 子コマンド名の前後の指定は同じオプションの繰り返し: 単一の値は後勝ち、リスト値は連結（12.3、5.1、5.4） */

NAOS_TEST(inherited_value_last_wins_across_levels)
{
    static const char *const tokens[] = {"nest", "-c", "a", "remote", "add", "-c", "b", "n", "u", "-t", "x"};
    inherit_run_t run;

    run_parse(&run, 11, tokens);
    check_ok(&run, test_nest_remote_add);
    NAOS_CHECK_PTR_EQ(run.argv[6], mnd_args_value(&run.result, "config"));
    NAOS_CHECK_TRUE(mnd_args_is_set(&run.result, "config"));
    NAOS_CHECK_PTR_EQ(run.argv[6], run.result.levels[0].values[1].value);
    run_free(&run);
}

NAOS_TEST(inherited_required_value_last_wins_across_levels)
{
    static const char *const tokens[] = {"nest", "remote", "-t", "a", "add", "--token=b", "n", "u"};
    inherit_run_t run;

    run_parse(&run, 8, tokens);
    check_ok(&run, test_nest_remote_add);
    NAOS_CHECK_STR_EQ("b", mnd_args_value(&run.result, "token"));
    run_free(&run);
}

NAOS_TEST(inherited_list_concatenates_across_levels)
{
    static const char *const tokens[] = {
        "nest", "remote", "--tags", "a", "add", "--tags", "b,c", "n", "u", "-t", "x"
    };
    inherit_run_t run;

    run_parse(&run, 11, tokens);
    check_ok(&run, test_nest_remote_add);
    NAOS_REQUIRE_UINT_EQ(3, mnd_args_list_count(&run.result, "tags"));
    NAOS_CHECK_STR_EQ("a", mnd_args_list_at(&run.result, "tags", 0));
    NAOS_CHECK_STR_EQ("b", mnd_args_list_at(&run.result, "tags", 1));
    NAOS_CHECK_STR_EQ("c", mnd_args_list_at(&run.result, "tags", 2));
    NAOS_CHECK_TRUE(mnd_args_is_set(&run.result, "tags"));
    NAOS_CHECK_UINT_EQ(3, run.result.levels[1].values[1].item_count);
    run_free(&run);
}

/* 必須の引き継ぐオプションは、経路上のどこで指定しても充足する（12.5、7.1） */

NAOS_TEST(inherited_required_after_remote_is_satisfied)
{
    static const char *const tokens[] = {"nest", "remote", "-t", "x", "add", "n", "u"};
    inherit_run_t run;

    run_parse(&run, 7, tokens);
    check_ok(&run, test_nest_remote_add);
    NAOS_CHECK_STR_EQ("x", mnd_args_value(&run.result, "token"));
    run_free(&run);
}

NAOS_TEST(inherited_required_after_add_is_satisfied)
{
    static const char *const tokens[] = {"nest", "remote", "add", "n", "u", "--token", "x"};
    inherit_run_t run;

    run_parse(&run, 7, tokens);
    check_ok(&run, test_nest_remote_add);
    NAOS_CHECK_STR_EQ("x", mnd_args_value(&run.result, "token"));
    run_free(&run);
}

NAOS_TEST(inherited_required_missing_is_error)
{
    static const char *const tokens[] = {"nest", "remote", "add", "n", "u"};
    inherit_run_t run;

    run_parse(&run, 5, tokens);
    check_usage_error(&run, "nest remote add: missing required option '--token'");
    NAOS_CHECK_PTR_EQ(test_nest_remote_add, mnd_args_command(&run.result));
    run_free(&run);
}

NAOS_TEST(inherited_required_missing_on_remove_is_error)
{
    static const char *const tokens[] = {"nest", "-v", "remote", "remove", "n"};
    inherit_run_t run;

    run_parse(&run, 5, tokens);
    check_usage_error(&run, "nest remote remove: missing required option '--token'");
    run_free(&run);
}

/* 既定値は、経路上のすべての段で、明示的に指定されていないオプションに適用する（12.6、5.6） */

NAOS_TEST(inherited_default_is_applied)
{
    static const char *const tokens[] = {"nest", "remote", "add", "n", "u", "-t", "x"};
    inherit_run_t run;

    run_parse(&run, 7, tokens);
    check_ok(&run, test_nest_remote_add);
    NAOS_CHECK_STR_EQ("nest.cfg", mnd_args_value(&run.result, "config"));
    NAOS_CHECK_FALSE(mnd_args_is_set(&run.result, "config"));
    NAOS_CHECK_FALSE(mnd_args_flag(&run.result, "verbose"));
    NAOS_CHECK_FALSE(mnd_args_is_set(&run.result, "tags"));
    NAOS_CHECK_UINT_EQ(0, mnd_args_list_count(&run.result, "tags"));
    run_free(&run);
}

NAOS_TEST(inherited_default_is_applied_on_status)
{
    static const char *const tokens[] = {"nest", "status"};
    inherit_run_t run;

    run_parse(&run, 2, tokens);
    check_ok(&run, test_nest_status);
    NAOS_CHECK_STR_EQ("nest.cfg", mnd_args_value(&run.result, "config"));
    NAOS_CHECK_FALSE(mnd_args_is_set(&run.result, "config"));
    run_free(&run);
}

/*
 * 必須の検査の順序を確かめるための宣言（プログラム名 "req"）。
 *   ルート: --alpha <A>（値、必須、引き継がない）、--gamma <G>（値、必須、引き継ぐ）
 *   sub: --beta <B>（値、必須）
 */
static const mnd_args_option_t req_root_options[] = {
    { .long_name = "alpha", .kind = MND_ARGS_VALUE, .value_name = "A", .required = true },
    { .long_name = "gamma", .kind = MND_ARGS_VALUE, .value_name = "G", .required = true,
      .inherited = true }
};

static const mnd_args_option_t req_sub_options[] = {
    { .long_name = "beta", .kind = MND_ARGS_VALUE, .value_name = "B", .required = true }
};

static const mnd_args_command_t req_commands[] = {
    {
        .name = "sub",
        .summary = "A leaf command",
        .options = req_sub_options,
        .option_count = sizeof req_sub_options / sizeof req_sub_options[0]
    }
};

static const mnd_args_command_t req_root = {
    .options = req_root_options,
    .option_count = sizeof req_root_options / sizeof req_root_options[0],
    .commands = req_commands,
    .command_count = sizeof req_commands / sizeof req_commands[0]
};

static const mnd_args_program_t req_program = {
    .name = "req",
    .version = "1.0",
    .root = &req_root
};

/* 必須の検査は経路上のすべての段を段 0 から順に、各段では宣言の順に見る（7.1） */

NAOS_TEST(required_check_starts_at_level_zero)
{
    static const char *const tokens[] = {"req", "sub"};
    inherit_run_t run;

    run_parse_with(&run, &req_program, 2, tokens);
    check_usage_error(&run, "req sub: missing required option '--alpha'");
    run_free(&run);
}

NAOS_TEST(required_check_follows_declaration_order_in_level)
{
    static const char *const tokens[] = {"req", "--alpha", "a", "sub"};
    inherit_run_t run;

    run_parse_with(&run, &req_program, 4, tokens);
    check_usage_error(&run, "req sub: missing required option '--gamma'");
    run_free(&run);
}

NAOS_TEST(required_check_reaches_last_level)
{
    static const char *const tokens[] = {"req", "--alpha", "a", "sub", "--gamma", "g"};
    inherit_run_t run;

    run_parse_with(&run, &req_program, 6, tokens);
    check_usage_error(&run, "req sub: missing required option '--beta'");
    run_free(&run);
}

NAOS_TEST(required_check_passes_when_all_levels_are_set)
{
    static const char *const tokens[] = {"req", "--alpha", "a", "sub", "--beta", "b", "--gamma", "g"};
    inherit_run_t run;

    run_parse_with(&run, &req_program, 8, tokens);
    check_ok(&run, &req_commands[0]);
    NAOS_CHECK_STR_EQ("a", mnd_args_value(&run.result, "alpha"));
    NAOS_CHECK_STR_EQ("b", mnd_args_value(&run.result, "beta"));
    NAOS_CHECK_STR_EQ("g", mnd_args_value(&run.result, "gamma"));
    run_free(&run);
}

NAOS_SUITE(inherit,
    NAOS_ADD_TEST(inherited_flag_before_remote),
    NAOS_ADD_TEST(inherited_flag_after_remote),
    NAOS_ADD_TEST(inherited_flag_after_add),
    NAOS_ADD_TEST(inherited_long_flag_after_add),
    NAOS_ADD_TEST(inherited_flag_repeated_across_levels),
    NAOS_ADD_TEST(non_inherited_option_after_remote_is_unknown),
    NAOS_ADD_TEST(non_inherited_option_after_add_is_unknown),
    NAOS_ADD_TEST(non_inherited_option_before_child_is_recorded),
    NAOS_ADD_TEST(remote_option_on_status_is_unknown),
    NAOS_ADD_TEST(remote_option_before_remote_is_unknown),
    NAOS_ADD_TEST(root_inherited_option_on_status_is_recorded),
    NAOS_ADD_TEST(inherited_value_last_wins_across_levels),
    NAOS_ADD_TEST(inherited_required_value_last_wins_across_levels),
    NAOS_ADD_TEST(inherited_list_concatenates_across_levels),
    NAOS_ADD_TEST(inherited_required_after_remote_is_satisfied),
    NAOS_ADD_TEST(inherited_required_after_add_is_satisfied),
    NAOS_ADD_TEST(inherited_required_missing_is_error),
    NAOS_ADD_TEST(inherited_required_missing_on_remove_is_error),
    NAOS_ADD_TEST(inherited_default_is_applied),
    NAOS_ADD_TEST(inherited_default_is_applied_on_status),
    NAOS_ADD_TEST(required_check_starts_at_level_zero),
    NAOS_ADD_TEST(required_check_follows_declaration_order_in_level),
    NAOS_ADD_TEST(required_check_reaches_last_level),
    NAOS_ADD_TEST(required_check_passes_when_all_levels_are_set)
)
