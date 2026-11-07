#ifndef MONDE_TESTS_CLI_TEST_UTIL_H
#define MONDE_TESTS_CLI_TEST_UTIL_H

/*
 * cli のテスト用の補助関数（テストの実行ファイルにだけ含め、製品のライブラリには含めない）。
 * ファイルの作成・読み戻し、入口の呼び出し、getenv / now の差し替えは 1.3 で加える
 */

/* プロセス内テストの作業ディレクトリ（CMake が構成時に作る。MONDE_CLI_TEST_WORK_DIR） */
const char *test_work_dir(void);

#endif
