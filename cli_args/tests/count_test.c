#include <naos/naos.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "clopts/clopts.h"
#include "clopts/errors.h"
#include "test_decl.h"

/* フラグの出現回数（15.1〜15.7）。プログラム ext を使う（11.1） */

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

/* tokens の argc 個を引数列として test_ext_program で解析する */
static void run_parse(count_run_t *run, int argc, const char *const *tokens)
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

    run->status = clopts_parse(&test_ext_program, argc, run->argv, &run->result, &run->errors);
    NAOS_CHECK_INT_EQ(run->status, run->result.status);

    for (i = 0; i < argc; i++)
    {
        NAOS_CHECK_PTR_EQ(run->storage[i], run->argv[i]);
        NAOS_CHECK_STR_EQ(tokens[i], run->argv[i]);
    }
    NAOS_CHECK_NULL(run->argv[argc]);
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

NAOS_SUITE(count,
    NAOS_ADD_TEST(unset_flags_have_zero_count)
)
