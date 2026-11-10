#include <naos/naos.h>

#include <stddef.h>
#include <string.h>

#include "cli_args/args.h"
#include "foundation/diag.h"
#include "test_decl.h"

/* 位置引数の記録、オプションとの混在、終端 "--"、位置引数の個数の検査（6.1〜6.5） */

#define POSITIONAL_MAX_ARGS 8
#define POSITIONAL_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct positional_run
{
    char storage[POSITIONAL_MAX_ARGS][POSITIONAL_MAX_LEN];
    char *argv[POSITIONAL_MAX_ARGS + 1];
    mnd_args_result_t result;
    mnd_diag_list_t diags;
    mnd_args_status_t status;
} positional_run_t;

/* tokens の argc 個を引数列として test_tool_program で解析する */
static void run_parse(positional_run_t *run, int argc, const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    mnd_args_result_init(&run->result);
    mnd_diag_list_init(&run->diags);

    NAOS_REQUIRE_TRUE(argc <= POSITIONAL_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < POSITIONAL_MAX_LEN);
        strcpy(run->storage[i], tokens[i]);
        run->argv[i] = run->storage[i];
    }
    run->argv[argc] = NULL;

    run->status = mnd_args_parse(&test_tool_program, argc, run->argv, &run->result, &run->diags);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
}

static void run_free(positional_run_t *run)
{
    mnd_args_result_free(&run->result);
    mnd_diag_list_free(&run->diags);
}

/* 診断なしで MND_ARGS_OK になり、command が選択されていることを確かめる */
static void check_ok(const positional_run_t *run, const mnd_args_command_t *command)
{
    NAOS_CHECK_INT_EQ(MND_ARGS_OK, run->status);
    NAOS_CHECK_PTR_EQ(command, mnd_args_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&run->diags));
}

/* 使い方のエラーで、期待するメッセージの診断がちょうど 1 件だけ追加されたことを確かめる */
static void check_usage_error(const positional_run_t *run, const mnd_args_command_t *command,
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

/* 位置引数がちょうど expected の順で記録され、引数列の要素を指していることを確かめる */
static void check_positionals(const positional_run_t *run, size_t count, const int *argv_indices)
{
    size_t i;

    NAOS_REQUIRE_UINT_EQ(count, mnd_args_positional_count(&run->result));
    for (i = 0; i < count; i++)
    {
        NAOS_CHECK_PTR_EQ(run->argv[argv_indices[i]], mnd_args_positional_at(&run->result, i));
    }
}

/* オプションと位置引数の混在（6.1、6.2） */

NAOS_TEST(mixed_order_gives_same_result)
{
    static const char *const first[] = {"tool", "build", "-v", "x", "-o", "y"};
    static const char *const second[] = {"tool", "build", "-o", "y", "x", "-v"};
    positional_run_t a;
    positional_run_t b;

    run_parse(&a, 6, first);
    run_parse(&b, 6, second);
    check_ok(&a, test_tool_build);
    check_ok(&b, test_tool_build);

    NAOS_CHECK_TRUE(mnd_args_flag(&a.result, "verbose"));
    NAOS_CHECK_TRUE(mnd_args_flag(&b.result, "verbose"));
    NAOS_CHECK_FALSE(mnd_args_flag(&a.result, "quiet"));
    NAOS_CHECK_FALSE(mnd_args_flag(&b.result, "quiet"));
    NAOS_CHECK_STR_EQ("y", mnd_args_value(&a.result, "output"));
    NAOS_CHECK_STR_EQ("y", mnd_args_value(&b.result, "output"));
    NAOS_CHECK_STR_EQ("1", mnd_args_value(&a.result, "level"));
    NAOS_CHECK_STR_EQ("1", mnd_args_value(&b.result, "level"));
    NAOS_CHECK_UINT_EQ(2, mnd_args_list_count(&a.result, "keys"));
    NAOS_CHECK_UINT_EQ(2, mnd_args_list_count(&b.result, "keys"));
    NAOS_REQUIRE_UINT_EQ(1, mnd_args_positional_count(&a.result));
    NAOS_REQUIRE_UINT_EQ(1, mnd_args_positional_count(&b.result));
    NAOS_CHECK_STR_EQ("x", mnd_args_positional_at(&a.result, 0));
    NAOS_CHECK_STR_EQ("x", mnd_args_positional_at(&b.result, 0));

    run_free(&a);
    run_free(&b);
}

NAOS_TEST(positionals_are_recorded_in_order)
{
    static const char *const tokens[] = {"tool", "cat", "a", "b", "c", "d"};
    static const int indices[] = {2, 3, 4, 5};
    positional_run_t run;

    run_parse(&run, 6, tokens);
    check_ok(&run, test_tool_cat);
    check_positionals(&run, 4, indices);
    run_free(&run);
}

NAOS_TEST(zero_positionals_are_accepted_when_minimum_is_zero)
{
    static const char *const tokens[] = {"tool", "cat"};
    positional_run_t run;

    run_parse(&run, 2, tokens);
    check_ok(&run, test_tool_cat);
    NAOS_CHECK_UINT_EQ(0, mnd_args_positional_count(&run.result));
    run_free(&run);
}

/* 終端 "--"（6.3） */

NAOS_TEST(tokens_after_terminator_are_positionals)
{
    static const char *const tokens[] = {"tool", "build", "-o", "y", "--", "-v"};
    static const int indices[] = {5};
    positional_run_t run;

    run_parse(&run, 6, tokens);
    check_ok(&run, test_tool_build);
    check_positionals(&run, 1, indices);
    NAOS_CHECK_STR_EQ("-v", mnd_args_positional_at(&run.result, 0));
    NAOS_CHECK_FALSE(mnd_args_flag(&run.result, "verbose"));
    NAOS_CHECK_FALSE(mnd_args_is_set(&run.result, "verbose"));
    run_free(&run);
}

NAOS_TEST(second_terminator_is_positional)
{
    static const char *const tokens[] = {"tool", "cat", "--", "a", "--", "b"};
    static const int indices[] = {3, 4, 5};
    positional_run_t run;

    run_parse(&run, 6, tokens);
    check_ok(&run, test_tool_cat);
    check_positionals(&run, 3, indices);
    NAOS_CHECK_STR_EQ("a", mnd_args_positional_at(&run.result, 0));
    NAOS_CHECK_STR_EQ("--", mnd_args_positional_at(&run.result, 1));
    NAOS_CHECK_STR_EQ("b", mnd_args_positional_at(&run.result, 2));
    run_free(&run);
}

NAOS_TEST(help_after_terminator_is_positional)
{
    static const char *const tokens[] = {"tool", "cat", "--", "--help", "-h"};
    static const int indices[] = {3, 4};
    positional_run_t run;

    run_parse(&run, 5, tokens);
    check_ok(&run, test_tool_cat);
    check_positionals(&run, 2, indices);
    run_free(&run);
}

NAOS_TEST(terminator_alone_gives_no_positionals)
{
    static const char *const tokens[] = {"tool", "cat", "--"};
    positional_run_t run;

    run_parse(&run, 3, tokens);
    check_ok(&run, test_tool_cat);
    NAOS_CHECK_UINT_EQ(0, mnd_args_positional_count(&run.result));
    run_free(&run);
}

/* 位置引数の不足（6.4） */

NAOS_TEST(missing_second_positional_names_it)
{
    static const char *const tokens[] = {"tool", "copy", "a"};
    positional_run_t run;

    run_parse(&run, 3, tokens);
    check_usage_error(&run, test_tool_copy, "tool copy: missing argument <DEST>");
    run_free(&run);
}

NAOS_TEST(missing_first_positional_names_it)
{
    static const char *const tokens[] = {"tool", "copy"};
    positional_run_t run;

    run_parse(&run, 2, tokens);
    check_usage_error(&run, test_tool_copy, "tool copy: missing argument <SRC>");
    run_free(&run);
}

/* 余分な位置引数（6.5） */

NAOS_TEST(extra_positional_is_unexpected)
{
    static const char *const tokens[] = {"tool", "copy", "a", "b", "c"};
    positional_run_t run;

    run_parse(&run, 5, tokens);
    check_usage_error(&run, test_tool_copy, "tool copy: unexpected argument 'c'");
    run_free(&run);
}

NAOS_TEST(first_extra_positional_is_reported)
{
    static const char *const tokens[] = {"tool", "copy", "a", "b", "c", "d"};
    positional_run_t run;

    run_parse(&run, 6, tokens);
    check_usage_error(&run, test_tool_copy, "tool copy: unexpected argument 'c'");
    run_free(&run);
}

NAOS_TEST(extra_positional_after_terminator_is_unexpected)
{
    static const char *const tokens[] = {"tool", "build", "-o", "y", "x", "--", "-v"};
    positional_run_t run;

    run_parse(&run, 7, tokens);
    check_usage_error(&run, test_tool_build, "tool build: unexpected argument '-v'");
    run_free(&run);
}

NAOS_SUITE(positional,
    NAOS_ADD_TEST(mixed_order_gives_same_result),
    NAOS_ADD_TEST(positionals_are_recorded_in_order),
    NAOS_ADD_TEST(zero_positionals_are_accepted_when_minimum_is_zero),
    NAOS_ADD_TEST(tokens_after_terminator_are_positionals),
    NAOS_ADD_TEST(second_terminator_is_positional),
    NAOS_ADD_TEST(help_after_terminator_is_positional),
    NAOS_ADD_TEST(terminator_alone_gives_no_positionals),
    NAOS_ADD_TEST(missing_second_positional_names_it),
    NAOS_ADD_TEST(missing_first_positional_names_it),
    NAOS_ADD_TEST(extra_positional_is_unexpected),
    NAOS_ADD_TEST(first_extra_positional_is_reported),
    NAOS_ADD_TEST(extra_positional_after_terminator_is_unexpected)
)
