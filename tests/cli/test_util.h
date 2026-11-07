#ifndef MONDE_TESTS_CLI_TEST_UTIL_H
#define MONDE_TESTS_CLI_TEST_UTIL_H

/*
 * cli のテスト用の補助関数（テストの実行ファイルにだけ含め、製品のライブラリには含めない）。
 * ファイル名は作業ディレクトリからの相対で受け取る。CTest はテストを別々のプロセスで並列に
 * 実行するため、テストごとに異なるファイル名（テスト名を含む名前など）を使う
 */

#include "cli/cli.h"
#include "foundation/buf.h"

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/* test_cli_run が env.version に渡すバージョン文字列 */
#define TEST_CLI_VERSION "0.0.0-test"

/* プロセス内テストの作業ディレクトリ（CMake が構成時に作る。MONDE_CLI_TEST_WORK_DIR） */
const char *test_work_dir(void);

/* 作業ディレクトリの name のパスを path に書く（内容を置き換える）。メモリ不足なら false */
bool test_work_path(const char *name, mnd_buf_t *path);

/* 作業ディレクトリの name に bytes の len バイトをバイナリモードで書く（既存の内容は置き換える） */
bool test_write_file(const char *name, const void *bytes, size_t len);

/* 作業ディレクトリの name の内容をバイナリモードで読み、out の内容を置き換える */
bool test_read_file(const char *name, mnd_buf_t *out);

/* 作業ディレクトリの name が読み込みで開けるなら true */
bool test_file_exists(const char *name);

/* 作業ディレクトリの name を削除する。もともと存在しない場合も true */
bool test_remove_file(const char *name);

/* test_cli_run の結果 */
typedef struct test_cli_result
{
    int exit_code;   /* mnd_cli_run の戻り値 */
    mnd_buf_t out;   /* out に書かれたバイト列 */
    mnd_buf_t err;   /* err に書かれたバイト列 */
} test_cli_result_t;

#define TEST_CLI_RESULT_INIT { -1, MND_BUF_INIT, MND_BUF_INIT }

void test_cli_result_free(test_cli_result_t *result);

/*
 * 作業ディレクトリの <name>.in（内容は stdin_bytes の stdin_len バイト。NULL なら空）、
 * <name>.out、<name>.err をバイナリモードで開いて env の in / out / err とし、
 * version を TEST_CLI_VERSION、getenv を test_env_getenv、now を test_clock_now にして
 * mnd_cli_run(env, argc, argv) を呼ぶ。終了コードと out / err の内容を result に入れる。
 * argv は argv[argc] == NULL の配列。補助関数自体のファイル操作に失敗したら false
 */
bool test_cli_run(const char *name, const void *stdin_bytes, size_t stdin_len,
                  int argc, char *const *argv, test_cli_result_t *result);

/* 環境変数の表の 1 行 */
typedef struct test_env_var
{
    const char *name;
    const char *value;
} test_env_var_t;

/*
 * test_env_getenv が引く表を設定する（表は呼び出し側が保持し続ける）。
 * vars == NULL または count == 0 で空の表にする。初期状態は空の表
 */
void test_env_set(const test_env_var_t *vars, size_t count);

/* mnd_cli_getenv_fn の差し替え: 表で name に一致する最初の行の値を返し、なければ NULL */
const char *test_env_getenv(const char *name);

/* test_clock_now が返す時刻を設定する。初期値は (time_t)0 */
void test_clock_set(time_t now);

/* mnd_cli_now_fn の差し替え: test_clock_set で設定した固定時刻を返す */
time_t test_clock_now(void);

#endif
