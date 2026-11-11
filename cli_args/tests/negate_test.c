#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/errors.h"
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

NAOS_SUITE(negate,
    NAOS_ADD_TEST(neg_declaration_is_valid),
    NAOS_ADD_TEST(positive_form_enables_negatable_flag),
    NAOS_ADD_TEST(unset_negatable_flag_is_disabled_and_not_set)
)
