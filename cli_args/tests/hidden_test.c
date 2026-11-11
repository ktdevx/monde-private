#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "clopts_internal.h"
#include "test_decl.h"

/* 隠しオプション（16.1〜16.5、16.7、16.8）。プログラム ext を使う（11.1） */

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

/* tokens の argc 個を引数列として test_ext_program で解析する */
static void run_parse(hidden_run_t *run, int argc, const char *const *tokens)
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

    run->status = clopts_parse(&test_ext_program, argc, run->argv, &run->result, &run->errors);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
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

NAOS_SUITE(hidden,
    NAOS_ADD_TEST(ext_declaration_is_valid),
    NAOS_ADD_TEST(hidden_options_are_recorded_by_their_names)
)
