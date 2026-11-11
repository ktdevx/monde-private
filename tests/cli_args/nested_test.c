#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

#include "cli_args/args.h"
#include "foundation/diag.h"
#include "test_decl.h"

/* 入れ子の構成（nest）の経路と診断の接頭辞（2.2、2.3、2.4、2.8、6.4、7.1、7.2、8.9） */

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
    mnd_diag_list_t diags;
    mnd_args_status_t status;
} nested_run_t;

/* tokens の argc 個を引数列として test_nest_program で解析する */
static void run_parse(nested_run_t *run, int argc, const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    mnd_args_result_init(&run->result);
    mnd_diag_list_init(&run->diags);

    NAOS_REQUIRE_TRUE(argc <= NESTED_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < NESTED_MAX_LEN);
        strcpy(run->storage[i], tokens[i]);
        run->argv[i] = run->storage[i];
    }
    run->argv[argc] = NULL;

    run->status = mnd_args_parse(&test_nest_program, argc, run->argv, &run->result, &run->diags);
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
    mnd_diag_list_free(&run->diags);
}

/* 診断なしで status になり、経路の段の数が depth で最後が command であることを確かめる */
static void check_status(const nested_run_t *run, mnd_args_status_t status, size_t depth,
                         const mnd_args_command_t *command)
{
    NAOS_CHECK_INT_EQ(status, run->status);
    NAOS_CHECK_UINT_EQ(depth, mnd_args_depth(&run->result));
    NAOS_CHECK_PTR_EQ(command, mnd_args_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&run->diags));
}

/*
 * 使い方のエラーで、経路の最後が command で、期待するメッセージの診断がちょうど 1 件だけ
 * 追加されたことを確かめる（7.2、7.4）
 */
static void check_usage_error(const nested_run_t *run, const mnd_args_command_t *command,
                              const char *expected)
{
    const mnd_diag_t *d;

    NAOS_CHECK_INT_EQ(MND_ARGS_USAGE_ERROR, run->status);
    NAOS_CHECK_PTR_EQ(command, mnd_args_command(&run->result));
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&run->diags));
    d = mnd_diag_list_at(&run->diags, 0);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_CHECK_NULL(d->path);
    NAOS_CHECK_UINT_EQ(0, d->line);
    NAOS_CHECK_STR_EQ(expected, d->message);
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

/* 診断の接頭辞は、誤りを検出した時点の経路になる（2.3、6.4、7.2） */

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
    NAOS_ADD_TEST(version_under_remote_is_unknown_option)
)
