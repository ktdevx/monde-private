#ifndef MONDE_TESTS_PO_MERGE_TEST_UTIL_H
#define MONDE_TESTS_PO_MERGE_TEST_UTIL_H

/*
 * po_merge のテスト用の補助関数（テストの実行ファイルにだけ含め、製品のライブラリには含めない）。
 * 補助関数は Naos の検査をその中で行い、失敗すればテストを失敗させる。致命的な失敗では
 * longjmp でテストを抜けるため、それまでに確保した領域は解放されない（テストでは許容する）。
 * 下の大文字のマクロを使うと、失敗の表示に呼び出し元のファイルと行が trace として加わる
 */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "po/po.h"

/*
 * PO の文字列 text（NUL 終端）を mnd_po_read で読み込み、catalog（未初期化でよい）に入れる。
 * 診断（警告を含む）があれば 1 つずつ表示してテストを失敗させ、読み込みに失敗したら
 * テストを止める。catalog は成功・失敗にかかわらず初期化され、解放は呼び出し側が行う
 */
void test_read_po(const char *text, mnd_po_catalog_t *catalog);

/*
 * catalog を既定の設定（折り返しあり、LF）で mnd_po_write により書き出し、expected
 * （NUL 終端）とバイト単位で比べる。差があれば、書き出した内容と expected の両方と
 * 最初に異なる位置を表示してテストを失敗させる。書き出しに失敗したらテストを止める
 */
void test_check_written(const mnd_po_catalog_t *catalog, const char *expected);

/*
 * テストデータのディレクトリ（MND_MERGE_TEST_DATA_DIR）にあるファイル name
 * （例: "exact/old.po"）を mnd_file_read で読み、BOM を除いた内容を content
 * （初期化済み）に入れる。読み込みに失敗したらテストを止める
 */
void test_read_data_file(const char *name, mnd_buf_t *content);

/* 呼び出し元の位置を trace として表示に加える形 */
#define TEST_READ_PO(text, catalog) \
    do \
    { \
        NAOS_TRACE_PUSH("in TEST_READ_PO"); \
        test_read_po((text), (catalog)); \
        NAOS_TRACE_POP(); \
    } while (0)

#define TEST_CHECK_WRITTEN(catalog, expected) \
    do \
    { \
        NAOS_TRACE_PUSH("in TEST_CHECK_WRITTEN"); \
        test_check_written((catalog), (expected)); \
        NAOS_TRACE_POP(); \
    } while (0)

#define TEST_READ_DATA_FILE(name, content) \
    do \
    { \
        NAOS_TRACE_PUSH("in TEST_READ_DATA_FILE(%s)", (name)); \
        test_read_data_file((name), (content)); \
        NAOS_TRACE_POP(); \
    } while (0)

#endif
