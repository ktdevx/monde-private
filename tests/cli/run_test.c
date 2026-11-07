/* スイート run: ヘルプ、バージョン、引数なし、使い方のエラー、終了コード（1.x、9.5〜9.9） */

#include <naos/naos.h>

#include "cli/cli.h"
#include "test_util.h"

#include <string.h>

/* 雛形: 終了コードの定数が 0 / 1 / 2 であり、作業ディレクトリのパスが渡されている */
NAOS_TEST(run_scaffold_exit_codes_and_work_dir)
{
    NAOS_CHECK_INT_EQ(0, MND_CLI_EXIT_OK);
    NAOS_CHECK_INT_EQ(1, MND_CLI_EXIT_FAILURE);
    NAOS_CHECK_INT_EQ(2, MND_CLI_EXIT_USAGE);
    NAOS_CHECK_STR_NE("", test_work_dir());
}

/* 雛形: 補助関数で仮の入口を呼ぶと、err に診断が出て 1 が返り、out は空である */
NAOS_TEST(run_scaffold_provisional_entry_reports_error)
{
    char arg0[] = "monde";
    char *argv[] = {arg0, NULL};
    test_cli_result_t result = TEST_CLI_RESULT_INIT;

    NAOS_REQUIRE_TRUE(test_cli_run("run_scaffold_provisional", NULL, 0, 1, argv, &result));
    NAOS_CHECK_INT_EQ(MND_CLI_EXIT_FAILURE, result.exit_code);
    NAOS_CHECK_UINT_EQ(0, result.out.len);
    NAOS_CHECK_TRUE(strstr(mnd_buf_cstr(&result.err), "monde: error: ") != NULL);
    test_cli_result_free(&result);
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

NAOS_SUITE(run,
    NAOS_ADD_TEST(run_scaffold_exit_codes_and_work_dir),
    NAOS_ADD_TEST(run_scaffold_provisional_entry_reports_error),
    NAOS_ADD_TEST(run_util_write_and_read_back_file),
    NAOS_ADD_TEST(run_util_getenv_looks_up_table),
    NAOS_ADD_TEST(run_util_clock_returns_fixed_time)
)
