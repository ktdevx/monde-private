#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "test_decl.h"

/* フラグの出現回数（15.1〜15.7）。プログラム tool、nest、neg、ext を使う（11.1） */

#define COUNT_MAX_ARGS 14
#define COUNT_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct count_run
{
    char storage[COUNT_MAX_ARGS][COUNT_MAX_LEN];
    char *argv[COUNT_MAX_ARGS + 1];
    clopts_result_t result;
    clopts_errors_t errors;
    clopts_status_t status;
} count_run_t;

/* tokens の argc 個を引数列として program で解析する */
static void run_parse_with(count_run_t *run, const clopts_program_t *program, int argc,
                           const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    clopts_result_init(&run->result);
    clopts_errors_init(&run->errors);

    NAOS_REQUIRE_TRUE(argc <= COUNT_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < COUNT_MAX_LEN);
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
static void run_parse(count_run_t *run, int argc, const char *const *tokens)
{
    run_parse_with(run, &test_ext_program, argc, tokens);
}

static void run_free(count_run_t *run)
{
    clopts_result_free(&run->result);
    clopts_errors_free(&run->errors);
}

/* エラーなしで成功し、経路の最後が command であることを確かめる */
static void check_ok(const count_run_t *run, const clopts_command_t *command)
{
    NAOS_CHECK_INT_EQ(CLOPTS_OK, run->status);
    NAOS_CHECK_PTR_EQ(command, clopts_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run->errors));
}

/* 指定のないフラグ（15.4） */

NAOS_TEST(unset_flags_have_zero_count)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t"};
    count_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "verbose"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "color"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "trace"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "force"));
    run_free(&run);
}

/* 否定できないフラグ（15.2、15.4、15.6）。プログラム tool を使う */

NAOS_TEST(bundled_short_flags_are_counted_one_each)
{
    static const char *const tokens[] = {"tool", "build", "-vvv", "--output", "o", "src"};
    count_run_t run;

    run_parse_with(&run, &test_tool_program, 6, tokens);
    check_ok(&run, test_tool_build);
    NAOS_CHECK_UINT_EQ(3, clopts_flag_count(&run.result, "verbose"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "verbose"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "verbose"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "quiet"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "quiet"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "quiet"));
    run_free(&run);
}

NAOS_TEST(long_short_and_bundled_forms_are_summed)
{
    static const char *const tokens[] = {"tool", "build", "-v", "--verbose", "-qv", "--output", "o",
                                         "src"};
    count_run_t run;

    run_parse_with(&run, &test_tool_program, 8, tokens);
    check_ok(&run, test_tool_build);
    NAOS_CHECK_UINT_EQ(3, clopts_flag_count(&run.result, "verbose"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "verbose"));
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "quiet"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "quiet"));
    run_free(&run);
}

NAOS_TEST(single_occurrence_counts_one)
{
    static const char *const tokens[] = {"tool", "build", "--output", "o", "--quiet", "src"};
    count_run_t run;

    run_parse_with(&run, &test_tool_program, 6, tokens);
    check_ok(&run, test_tool_build);
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "quiet"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "quiet"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "verbose"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "verbose"));
    run_free(&run);
}

NAOS_TEST(unspecified_non_negatable_flags_count_zero)
{
    static const char *const tokens[] = {"tool", "build", "--output", "o", "src"};
    count_run_t run;

    run_parse_with(&run, &test_tool_program, 5, tokens);
    check_ok(&run, test_tool_build);
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "verbose"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "verbose"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "quiet"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "quiet"));
    run_free(&run);
}

/* 否定できるフラグ（15.3、15.4、15.6）。設計書の例の表の 6 行。プログラム neg を使う */

NAOS_TEST(negatable_positive_forms_are_summed)
{
    static const char *const tokens[] = {"neg", "-c", "-c", "run", "-c"};
    count_run_t run;

    run_parse_with(&run, &test_neg_program, 5, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_UINT_EQ(3, clopts_flag_count(&run.result, "color"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "color"));
    run_free(&run);
}

NAOS_TEST(negated_form_resets_count_and_later_forms_are_counted)
{
    static const char *const tokens[] = {"neg", "-c", "-c", "--no-color", "run", "-c"};
    count_run_t run;

    run_parse_with(&run, &test_neg_program, 6, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "color"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "color"));
    run_free(&run);
}

NAOS_TEST(negated_form_last_counts_zero)
{
    static const char *const tokens[] = {"neg", "-c", "--no-color", "run"};
    count_run_t run;

    run_parse_with(&run, &test_neg_program, 4, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "color"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "color"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "color"));
    run_free(&run);
}

NAOS_TEST(unspecified_default_enabled_flag_counts_one)
{
    static const char *const tokens[] = {"neg", "run"};
    count_run_t run;

    run_parse_with(&run, &test_neg_program, 2, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "color"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "color"));
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "pager"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "pager"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "pager"));

    /* 選択されたコマンドが宣言したフラグも同じ（sync は既定で有効、force は違う） */
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "sync"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "sync"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "force"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "force"));
    run_free(&run);
}

NAOS_TEST(specified_default_enabled_flag_counts_positive_forms)
{
    static const char *const tokens[] = {"neg", "--pager", "--pager", "run"};
    count_run_t run;

    run_parse_with(&run, &test_neg_program, 4, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_UINT_EQ(2, clopts_flag_count(&run.result, "pager"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "pager"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "pager"));
    run_free(&run);
}

NAOS_TEST(negated_default_enabled_flag_counts_zero)
{
    static const char *const tokens[] = {"neg", "--no-pager", "run"};
    count_run_t run;

    run_parse_with(&run, &test_neg_program, 3, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "pager"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "pager"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "pager"));
    run_free(&run);
}

/* 引き継ぐフラグ（15.5）。子コマンドの名前の前と後の指定を合わせて数える */

NAOS_TEST(inherited_flag_is_summed_across_command_names)
{
    static const char *const tokens[] = {"nest", "-v", "remote", "-v", "--token", "t", "add", "-v",
                                         "n", "u"};
    count_run_t run;

    run_parse_with(&run, &test_nest_program, 10, tokens);
    check_ok(&run, test_nest_remote_add);
    NAOS_CHECK_UINT_EQ(3, clopts_flag_count(&run.result, "verbose"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "verbose"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "force"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "force"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "dry-run"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "dry-run"));
    run_free(&run);
}

NAOS_TEST(inherited_negatable_flag_is_reset_after_command_name)
{
    static const char *const tokens[] = {"neg", "-c", "run", "-c", "--no-color", "-c"};
    count_run_t run;

    run_parse_with(&run, &test_neg_program, 6, tokens);
    check_ok(&run, test_neg_run);
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "color"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "color"));
    run_free(&run);
}

/* 0 を返す場合（15.7、17.8） */

NAOS_TEST(value_and_list_options_count_zero)
{
    static const char *const tokens[] = {"tool", "build", "--output", "a", "-o", "b", "--level",
                                         "2", "--keys", "x", "-k", "y,z", "src"};
    count_run_t run;

    run_parse_with(&run, &test_tool_program, 13, tokens);
    check_ok(&run, test_tool_build);

    /* 指定の回数によらず 0。値と項目は今までどおり取得できる */
    NAOS_CHECK_STR_EQ("b", clopts_value(&run.result, "output"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "output"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "output"));
    NAOS_CHECK_STR_EQ("2", clopts_value(&run.result, "level"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "level"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "level"));
    NAOS_CHECK_UINT_EQ(3, clopts_list_count(&run.result, "keys"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "keys"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "keys"));
    run_free(&run);
}

NAOS_TEST(defaulted_value_and_list_options_count_zero)
{
    static const char *const tokens[] = {"tool", "build", "--output", "o", "src"};
    count_run_t run;

    run_parse_with(&run, &test_tool_program, 5, tokens);
    check_ok(&run, test_tool_build);

    /* 既定値が適用された値付きオプションとリスト値も 0 */
    NAOS_CHECK_STR_EQ("1", clopts_value(&run.result, "level"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "level"));
    NAOS_CHECK_UINT_EQ(2, clopts_list_count(&run.result, "keys"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "keys"));
    run_free(&run);
}

NAOS_TEST(undeclared_alias_and_null_names_count_zero)
{
    static const char *const tokens[] = {"ext", "-v", "run", "--token", "t", "--force"};
    count_run_t run;

    run_parse(&run, 6, tokens);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "verbose"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "verbose"));
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "force"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "force"));

    /* 経路上で宣言されていない名前 */
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "bogus"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "bogus"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "v"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, ""));

    /* 別名は、宣言されていない名前と同じ（17.8） */
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "chatty"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "chatty"));

    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, NULL));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, NULL));
    run_free(&run);
}

NAOS_TEST(unparsed_result_counts_zero)
{
    clopts_result_t initialized = CLOPTS_RESULT_INIT;
    clopts_result_t result;

    clopts_result_init(&result);
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&result, "verbose"));
    NAOS_CHECK_FALSE(clopts_flag(&result, "verbose"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&result, NULL));
    clopts_result_free(&result);

    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&initialized, "pager"));
    NAOS_CHECK_FALSE(clopts_flag(&initialized, "pager"));
    clopts_result_free(&initialized);
}

/* 状態が成功以外のとき。既定は成功のときだけ適用する（15.4、15.6） */

NAOS_TEST(help_request_does_not_apply_default_count)
{
    static const char *const tokens[] = {"neg", "--help"};
    count_run_t run;

    run_parse_with(&run, &test_neg_program, 2, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_HELP, run.status);
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "pager"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "pager"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "color"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "color"));
    run_free(&run);
}

NAOS_TEST(usage_error_does_not_apply_default_count)
{
    static const char *const tokens[] = {"neg", "run", "--bogus"};
    count_run_t run;

    run_parse_with(&run, &test_neg_program, 3, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_USAGE_ERROR, run.status);
    NAOS_CHECK_UINT_EQ(1, clopts_errors_count(&run.errors));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "pager"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "pager"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "sync"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "sync"));
    run_free(&run);
}

NAOS_TEST(flags_before_help_request_are_counted)
{
    static const char *const tokens[] = {"neg", "-c", "--help"};
    count_run_t run;

    run_parse_with(&run, &test_neg_program, 3, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_HELP, run.status);
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "color"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "color"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "pager"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "pager"));
    run_free(&run);
}

NAOS_SUITE(count,
    NAOS_ADD_TEST(unset_flags_have_zero_count),
    NAOS_ADD_TEST(bundled_short_flags_are_counted_one_each),
    NAOS_ADD_TEST(long_short_and_bundled_forms_are_summed),
    NAOS_ADD_TEST(single_occurrence_counts_one),
    NAOS_ADD_TEST(unspecified_non_negatable_flags_count_zero),
    NAOS_ADD_TEST(negatable_positive_forms_are_summed),
    NAOS_ADD_TEST(negated_form_resets_count_and_later_forms_are_counted),
    NAOS_ADD_TEST(negated_form_last_counts_zero),
    NAOS_ADD_TEST(unspecified_default_enabled_flag_counts_one),
    NAOS_ADD_TEST(specified_default_enabled_flag_counts_positive_forms),
    NAOS_ADD_TEST(negated_default_enabled_flag_counts_zero),
    NAOS_ADD_TEST(inherited_flag_is_summed_across_command_names),
    NAOS_ADD_TEST(inherited_negatable_flag_is_reset_after_command_name),
    NAOS_ADD_TEST(value_and_list_options_count_zero),
    NAOS_ADD_TEST(defaulted_value_and_list_options_count_zero),
    NAOS_ADD_TEST(undeclared_alias_and_null_names_count_zero),
    NAOS_ADD_TEST(unparsed_result_counts_zero),
    NAOS_ADD_TEST(help_request_does_not_apply_default_count),
    NAOS_ADD_TEST(usage_error_does_not_apply_default_count),
    NAOS_ADD_TEST(flags_before_help_request_are_counted)
)
