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

NAOS_SUITE(alias,
    NAOS_ADD_TEST(alias_name_gets_same_result_as_undeclared_name)
)
