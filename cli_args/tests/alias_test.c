#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "test_decl.h"

/* オプションの別名（17.1〜17.11）。プログラム ext を使う（11.1） */

#define ALIAS_MAX_ARGS 14
#define ALIAS_MAX_LEN 32

/*
 * 1 回の解析の入力と出力。引数列は書き換え可能な領域にコピーして渡し、
 * 解析の後に内容とポインタが変わっていないことを確かめる（10.5）
 */
typedef struct alias_run
{
    char storage[ALIAS_MAX_ARGS][ALIAS_MAX_LEN];
    char *argv[ALIAS_MAX_ARGS + 1];
    clopts_result_t result;
    clopts_errors_t errors;
    clopts_status_t status;
} alias_run_t;

/* tokens の argc 個を引数列として test_ext_program で解析する */
static void run_parse(alias_run_t *run, int argc, const char *const *tokens)
{
    int i;

    memset(run, 0, sizeof *run);
    clopts_result_init(&run->result);
    clopts_errors_init(&run->errors);

    NAOS_REQUIRE_TRUE(argc <= ALIAS_MAX_ARGS);
    for (i = 0; i < argc; i++)
    {
        NAOS_REQUIRE_TRUE(strlen(tokens[i]) < ALIAS_MAX_LEN);
        strcpy(run->storage[i], tokens[i]);
        run->argv[i] = run->storage[i];
    }
    run->argv[argc] = NULL;

    run->status = clopts_parse(&test_ext_program, argc, run->argv, &run->result, &run->errors);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
}

static void run_free(alias_run_t *run)
{
    clopts_result_free(&run->result);
    clopts_errors_free(&run->errors);
}

/* エラーなしで成功し、経路の最後が command であることを確かめる */
static void check_ok(const alias_run_t *run, const clopts_command_t *command)
{
    NAOS_CHECK_INT_EQ(CLOPTS_OK, run->status);
    NAOS_CHECK_PTR_EQ(command, clopts_command(&run->result));
    NAOS_CHECK_UINT_EQ(0, clopts_errors_count(&run->errors));
}

/* tokens を解析し、誤りが 1 件で、その文面が expected であることを確かめる */
static void check_error(int argc, const char *const *tokens, const clopts_command_t *command,
                        const char *expected)
{
    alias_run_t run;

    run_parse(&run, argc, tokens);
    NAOS_CHECK_INT_EQ(CLOPTS_USAGE_ERROR, run.status);
    NAOS_CHECK_PTR_EQ(command, clopts_command(&run.result));
    NAOS_REQUIRE_UINT_EQ(1, clopts_errors_count(&run.errors));
    NAOS_CHECK_STR_EQ(expected, clopts_errors_at(&run.errors, 0));
    run_free(&run);
}

/* tokens を解析して成功し、output の値が expected であることを確かめる */
static void check_output(int argc, const char *const *tokens, const char *expected)
{
    alias_run_t run;

    run_parse(&run, argc, tokens);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "output"));
    NAOS_CHECK_STR_EQ(expected, clopts_value(&run.result, "output"));
    run_free(&run);
}

/* tokens を解析して成功し、color が指定済みで、有効かどうかが enabled であることを確かめる */
static void check_color(int argc, const char *const *tokens, bool enabled)
{
    alias_run_t run;

    run_parse(&run, argc, tokens);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "color"));
    NAOS_CHECK_INT_EQ(enabled, clopts_flag(&run.result, "color"));
    NAOS_CHECK_UINT_EQ(enabled ? 1 : 0, clopts_flag_count(&run.result, "color"));
    run_free(&run);
}

/* 照合（17.2、17.6）。別名による指定は、元の名前による指定と同じオプションの指定になる */

NAOS_TEST(alias_with_separate_value_sets_option)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--out", "x"};

    check_output(6, tokens, "x");
}

NAOS_TEST(alias_with_attached_value_sets_option)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--out=x"};

    check_output(5, tokens, "x");
}

NAOS_TEST(second_alias_sets_option)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--outfile", "x"};

    check_output(6, tokens, "x");
}

NAOS_TEST(alias_satisfies_required_option)
{
    static const char *const tokens[] = {"ext", "run", "--key", "t"};
    alias_run_t run;

    run_parse(&run, 4, tokens);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "token"));
    NAOS_CHECK_STR_EQ("t", clopts_value(&run.result, "token"));
    run_free(&run);
}

NAOS_TEST(alias_sets_list_items)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--labels", "a,b"};
    alias_run_t run;

    run_parse(&run, 6, tokens);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "tags"));
    NAOS_REQUIRE_UINT_EQ(2, clopts_list_count(&run.result, "tags"));
    NAOS_CHECK_STR_EQ("a", clopts_list_at(&run.result, "tags", 0));
    NAOS_CHECK_STR_EQ("b", clopts_list_at(&run.result, "tags", 1));
    run_free(&run);
}

/* 繰り返し（17.3）。元の名前、短い名前、別名を混ぜても、今までの繰り返しの規則に従う */

NAOS_TEST(alias_after_long_name_wins)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--output", "a", "--out",
                                         "b"};

    check_output(8, tokens, "b");
}

NAOS_TEST(short_name_after_alias_wins)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--out", "a", "-o", "b"};

    check_output(8, tokens, "b");
}

NAOS_TEST(list_items_of_long_name_and_alias_are_joined)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--tags", "a", "--labels",
                                         "b,c"};
    alias_run_t run;

    run_parse(&run, 8, tokens);
    check_ok(&run, test_ext_run);
    NAOS_REQUIRE_UINT_EQ(3, clopts_list_count(&run.result, "tags"));
    NAOS_CHECK_STR_EQ("a", clopts_list_at(&run.result, "tags", 0));
    NAOS_CHECK_STR_EQ("b", clopts_list_at(&run.result, "tags", 1));
    NAOS_CHECK_STR_EQ("c", clopts_list_at(&run.result, "tags", 2));
    run_free(&run);
}

/* 引き継ぎ（17.4）。引き継ぐオプションの別名は、子コマンドの名前の前でも後でも同じ指定になる */

NAOS_TEST(inherited_alias_works_before_and_after_command_name)
{
    static const char *const after[] = {"ext", "run", "--token", "t", "--chatty"};
    static const char *const before[] = {"ext", "--chatty", "run", "--token", "t"};
    alias_run_t run;

    run_parse(&run, 5, after);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "verbose"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "verbose"));
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "verbose"));
    NAOS_CHECK_STR_EQ("t", clopts_value(&run.result, "token"));
    run_free(&run);

    run_parse(&run, 5, before);
    check_ok(&run, test_ext_run);
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "verbose"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "verbose"));
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "verbose"));
    NAOS_CHECK_STR_EQ("t", clopts_value(&run.result, "token"));
    run_free(&run);
}

/* 否定の形（17.5、14.4）。--no-<別名> は否定の形になり、最後の指定が有効になる */

NAOS_TEST(alias_enables_negatable_flag)
{
    static const char *const tokens[] = {"ext", "--colour", "run", "--token", "t"};

    check_color(5, tokens, true);
}

NAOS_TEST(negated_alias_disables_negatable_flag)
{
    static const char *const tokens[] = {"ext", "--no-colour", "run", "--token", "t"};

    check_color(5, tokens, false);
}

NAOS_TEST(negated_alias_after_long_name_disables)
{
    static const char *const tokens[] = {"ext", "--color", "--no-colour", "run", "--token", "t"};

    check_color(6, tokens, false);
}

NAOS_TEST(alias_after_negated_long_name_enables)
{
    static const char *const tokens[] = {"ext", "--no-color", "run", "--colour", "--token", "t"};

    check_color(6, tokens, true);
}

/* 取得（17.7、17.8）。解析結果は元の長い名前で求める。別名の名前では、宣言のない名前と同じ結果になる */

NAOS_TEST(alias_name_gets_same_result_as_undeclared_name)
{
    static const char *const tokens[] = {"ext", "--color", "-v", "run", "--token", "t",
                                         "--output", "x", "--tags", "a,b"};
    alias_run_t run;

    run_parse(&run, 10, tokens);
    check_ok(&run, test_ext_run);

    /* 元の名前では値が取れる */
    NAOS_CHECK_STR_EQ("x", clopts_value(&run.result, "output"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "token"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "color"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "verbose"));
    NAOS_CHECK_UINT_EQ(2, clopts_list_count(&run.result, "tags"));

    /* 宣言のない名前 */
    NAOS_CHECK_NULL(clopts_value(&run.result, "nosuch"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "nosuch"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "nosuch"));
    NAOS_CHECK_UINT_EQ(0, clopts_list_count(&run.result, "nosuch"));

    /* 別名の名前 */
    NAOS_CHECK_NULL(clopts_value(&run.result, "out"));
    NAOS_CHECK_NULL(clopts_value(&run.result, "outfile"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "out"));
    NAOS_CHECK_NULL(clopts_value(&run.result, "key"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "key"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "colour"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "colour"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "chatty"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "chatty"));
    NAOS_CHECK_UINT_EQ(0, clopts_list_count(&run.result, "labels"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "labels"));
    run_free(&run);
}

/* 別名で指定した後でも、別名の名前では宣言のない名前と同じ結果になる。元の名前では値が取れる */
NAOS_TEST(alias_name_gets_undeclared_result_after_alias_was_used)
{
    static const char *const tokens[] = {"ext", "--colour", "--chatty", "run", "--key", "t",
                                         "--out", "x", "--labels", "a,b"};
    alias_run_t run;

    run_parse(&run, 10, tokens);
    check_ok(&run, test_ext_run);

    /* 元の名前 */
    NAOS_CHECK_STR_EQ("x", clopts_value(&run.result, "output"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "output"));
    NAOS_CHECK_STR_EQ("t", clopts_value(&run.result, "token"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "token"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "color"));
    NAOS_CHECK_TRUE(clopts_is_set(&run.result, "color"));
    NAOS_CHECK_TRUE(clopts_flag(&run.result, "verbose"));
    NAOS_CHECK_UINT_EQ(1, clopts_flag_count(&run.result, "verbose"));
    NAOS_REQUIRE_UINT_EQ(2, clopts_list_count(&run.result, "tags"));
    NAOS_CHECK_STR_EQ("a", clopts_list_at(&run.result, "tags", 0));
    NAOS_CHECK_STR_EQ("b", clopts_list_at(&run.result, "tags", 1));

    /* 別名の名前 */
    NAOS_CHECK_NULL(clopts_value(&run.result, "out"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "out"));
    NAOS_CHECK_NULL(clopts_value(&run.result, "outfile"));
    NAOS_CHECK_NULL(clopts_value(&run.result, "key"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "key"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "colour"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "colour"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "colour"));
    NAOS_CHECK_FALSE(clopts_flag(&run.result, "chatty"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "chatty"));
    NAOS_CHECK_UINT_EQ(0, clopts_flag_count(&run.result, "chatty"));
    NAOS_CHECK_UINT_EQ(0, clopts_list_count(&run.result, "labels"));
    NAOS_CHECK_FALSE(clopts_is_set(&run.result, "labels"));
    run_free(&run);
}

/* エラー（17.9）。別名による指定の誤りは、利用者が書いた別名で示す */

NAOS_TEST(alias_without_value_is_an_error_naming_alias)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--out"};

    check_error(5, tokens, test_ext_run, "ext run: option '--out' requires a value");
}

NAOS_TEST(flag_alias_with_value_is_an_error_naming_alias)
{
    static const char *const tokens[] = {"ext", "--chatty=1", "run"};

    check_error(3, tokens, test_ext_program.root, "ext: option '--chatty' does not take a value");
}

NAOS_TEST(negated_alias_with_value_is_an_error_naming_alias)
{
    static const char *const tokens[] = {"ext", "--no-colour=1", "run"};

    check_error(3, tokens, test_ext_program.root,
                "ext: option '--no-colour' does not take a value");
}

NAOS_TEST(list_alias_with_empty_item_is_an_error_naming_alias)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--labels", "a,,b"};

    check_error(6, tokens, test_ext_run, "ext run: option '--labels' has an empty item in 'a,,b'");
}

/* 必須の欠落（17.10）。元の名前で示す */

NAOS_TEST(missing_required_option_is_named_by_long_name)
{
    static const char *const tokens[] = {"ext", "run"};

    check_error(2, tokens, test_ext_run, "ext run: missing required option '--token'");
}

/* 未知のオプション（3.6、3.7、14.8）。別名も完全に一致するときだけ受け付ける */

NAOS_TEST(alias_prefix_is_unknown)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--ou", "x"};

    check_error(6, tokens, test_ext_run, "ext run: unknown option '--ou'");
}

NAOS_TEST(alias_in_different_case_is_unknown)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--Out", "x"};

    check_error(6, tokens, test_ext_run, "ext run: unknown option '--Out'");
}

NAOS_TEST(negated_alias_of_value_option_is_unknown)
{
    static const char *const tokens[] = {"ext", "run", "--token", "t", "--no-out"};

    check_error(5, tokens, test_ext_run, "ext run: unknown option '--no-out'");
}

NAOS_TEST(negated_alias_of_non_negatable_flag_is_unknown)
{
    static const char *const tokens[] = {"ext", "--no-chatty", "run", "--token", "t"};

    check_error(5, tokens, test_ext_program.root, "ext: unknown option '--no-chatty'");
}

NAOS_SUITE(alias,
    NAOS_ADD_TEST(alias_with_separate_value_sets_option),
    NAOS_ADD_TEST(alias_with_attached_value_sets_option),
    NAOS_ADD_TEST(second_alias_sets_option),
    NAOS_ADD_TEST(alias_satisfies_required_option),
    NAOS_ADD_TEST(alias_sets_list_items),
    NAOS_ADD_TEST(alias_after_long_name_wins),
    NAOS_ADD_TEST(short_name_after_alias_wins),
    NAOS_ADD_TEST(list_items_of_long_name_and_alias_are_joined),
    NAOS_ADD_TEST(inherited_alias_works_before_and_after_command_name),
    NAOS_ADD_TEST(alias_enables_negatable_flag),
    NAOS_ADD_TEST(negated_alias_disables_negatable_flag),
    NAOS_ADD_TEST(negated_alias_after_long_name_disables),
    NAOS_ADD_TEST(alias_after_negated_long_name_enables),
    NAOS_ADD_TEST(alias_name_gets_same_result_as_undeclared_name),
    NAOS_ADD_TEST(alias_name_gets_undeclared_result_after_alias_was_used),
    NAOS_ADD_TEST(alias_without_value_is_an_error_naming_alias),
    NAOS_ADD_TEST(flag_alias_with_value_is_an_error_naming_alias),
    NAOS_ADD_TEST(negated_alias_with_value_is_an_error_naming_alias),
    NAOS_ADD_TEST(list_alias_with_empty_item_is_an_error_naming_alias),
    NAOS_ADD_TEST(missing_required_option_is_named_by_long_name),
    NAOS_ADD_TEST(alias_prefix_is_unknown),
    NAOS_ADD_TEST(alias_in_different_case_is_unknown),
    NAOS_ADD_TEST(negated_alias_of_value_option_is_unknown),
    NAOS_ADD_TEST(negated_alias_of_non_negatable_flag_is_unknown)
)
