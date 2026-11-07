#ifndef MONDE_TESTS_TU_TEST_UTIL_H
#define MONDE_TESTS_TU_TEST_UTIL_H

/*
 * tu のテスト用の補助関数（テストの実行ファイルにだけ含め、製品のライブラリには含めない）。
 * 補助関数は Naos の検査をその中で行い、失敗すればテストを失敗させる。致命的な失敗では
 * longjmp でテストを抜けるため、それまでに確保した領域は解放されない（テストでは許容する）。
 * 下の大文字のマクロを使うと、失敗の表示に呼び出し元のファイルと行が trace として加わる
 */

#include <naos/naos.h>

#include "foundation/buf.h"
#include "po/po.h"
#include "tu/tu.h"

#include <stdbool.h>
#include <stddef.h>

/* セグメントの期待値。span は [start, end) */
typedef struct test_segment_expect
{
    mnd_tu_origin_t origin;
    size_t line;
    size_t start;
    size_t end;
    const char *msgid;  /* NUL 終端 */
    const char *kind;   /* NUL 終端 */
} test_segment_expect_t;

/*
 * text（NUL 終端。長さは strlen）を、キーパスの一覧 keys[0..key_count)（key_count == 0 なら
 * NULL 可）とパス "test.md" で mnd_tu_scan により走査し、文書を返す。
 * 診断（警告を含む）があれば 1 つずつ表示してテストを失敗させ、走査に失敗したらテストを止める。
 * 文書は text を指すため、呼び出し側は mnd_tu_doc_free まで text を保持し、解放も行う
 */
mnd_tu_doc_t *test_scan(const char *text, const char *const *keys, size_t key_count);

/*
 * PO の文字列 text（NUL 終端）を mnd_po_read で読み込み、catalog（未初期化でよい）に入れる。
 * 診断（警告を含む）があれば 1 つずつ表示してテストを失敗させ、読み込みに失敗したら
 * テストを止める。catalog は成功・失敗にかかわらず初期化され、解放は呼び出し側が行う
 */
void test_read_po(const char *text, mnd_po_catalog_t *catalog);

/*
 * actual[0..actual_len) と expected[0..expected_len) を比べ、異なれば最初に異なる位置
 * （一方が他方の接頭辞なら短い方の長さ）を *offset に入れて true を返す。同じなら false
 * （*offset は変えない）。テストを失敗させない下位の関数
 */
bool test_find_difference(const char *actual, size_t actual_len,
                          const char *expected, size_t expected_len, size_t *offset);

/*
 * 2 つのバイト列が異なれば、最初に異なる位置・その行番号（expected の 1 起点）・両方の長さと、
 * その前後（最大 16 バイトずつ、制御文字などはエスケープ）を 1 行の説明として out（初期化済み。
 * 内容は捨てる）に入れ、true を返す。同じなら out を空にして false を返す。
 * テストを失敗させない下位の関数（メモリ不足ならテストを止める）
 */
bool test_describe_difference(const char *actual, size_t actual_len,
                              const char *expected, size_t expected_len, mnd_buf_t *out);

/*
 * 2 つのバイト列がバイト単位で同じことを検査する。異なれば test_describe_difference の説明を
 * 表示してテストを失敗させる（止めない）
 */
void test_check_bytes(const char *actual, size_t actual_len,
                      const char *expected, size_t expected_len);

/*
 * doc を catalog と options（NULL 可）で mnd_tu_translate により再構成し、出力が
 * expected[0..expected_len) とバイト単位で同じことを検査する（test_check_bytes）。
 * 診断（警告を含む）があれば 1 つずつ表示してテストを失敗させ、再構成に失敗したらテストを止める。
 * stats が NULL でなければ統計を入れる。警告を期待するテストは mnd_tu_translate を直接呼ぶ
 */
void test_check_translated(const mnd_tu_doc_t *doc, const mnd_po_catalog_t *catalog,
                           const mnd_tu_translate_options_t *options,
                           const char *expected, size_t expected_len, mnd_tu_stats_t *stats);

/*
 * doc のセグメントの列が expected[0..count)（count == 0 なら NULL 可）と同じことを検査する。
 * 件数と、各セグメントの origin・行番号・範囲・msgid・種類を比べ、異なるものはセグメントの
 * 番号を trace に加えて表示し、テストを失敗させる（止めない）
 */
void test_check_segments(const mnd_tu_doc_t *doc, const test_segment_expect_t *expected, size_t count);

/* 呼び出し元の位置を trace として表示に加える形 */
#define TEST_SCAN(text, keys, key_count, out) \
    do \
    { \
        NAOS_TRACE_PUSH("in TEST_SCAN"); \
        (out) = test_scan((text), (keys), (key_count)); \
        NAOS_TRACE_POP(); \
    } while (0)

#define TEST_READ_PO(text, catalog) \
    do \
    { \
        NAOS_TRACE_PUSH("in TEST_READ_PO"); \
        test_read_po((text), (catalog)); \
        NAOS_TRACE_POP(); \
    } while (0)

#define TEST_CHECK_BYTES(actual, actual_len, expected, expected_len) \
    do \
    { \
        NAOS_TRACE_PUSH("in TEST_CHECK_BYTES"); \
        test_check_bytes((actual), (actual_len), (expected), (expected_len)); \
        NAOS_TRACE_POP(); \
    } while (0)

#define TEST_CHECK_TRANSLATED(doc, catalog, options, expected, expected_len, stats) \
    do \
    { \
        NAOS_TRACE_PUSH("in TEST_CHECK_TRANSLATED"); \
        test_check_translated((doc), (catalog), (options), (expected), (expected_len), (stats)); \
        NAOS_TRACE_POP(); \
    } while (0)

#define TEST_CHECK_SEGMENTS(doc, expected, count) \
    do \
    { \
        NAOS_TRACE_PUSH("in TEST_CHECK_SEGMENTS"); \
        test_check_segments((doc), (expected), (count)); \
        NAOS_TRACE_POP(); \
    } while (0)

#endif
