#ifndef MONDE_TESTS_YAML_TEST_ALLOC_H
#define MONDE_TESTS_YAML_TEST_ALLOC_H

/*
 * テスト用の確保関数（n 回目の確保で失敗する、確保と解放の回数を数える）。
 * テストの実行ファイルにだけ含め、製品のライブラリには含めない
 */

#include "yaml/yaml_parse.h"

#include <stddef.h>

/*
 * 確保関数の状態。allocator の ctx はこの構造体を指す。
 * 構造体を移動（複製）すると ctx が古い位置を指すので、初期化したあとは移動しない
 */
typedef struct mnd_yaml_test_alloc
{
    mnd_yaml_allocator_t allocator;
    size_t fail_at;     /* 何回目（1 起点）の確保で失敗させるか。0 なら失敗させない */
    size_t calls;       /* alloc の呼び出し回数（失敗した呼び出しを含む） */
    size_t allocs;      /* 成功した確保の回数 */
    size_t frees;       /* NULL でないポインタの解放の回数（free(NULL) は数えない） */
    size_t failures;    /* 失敗させた確保の回数 */
} mnd_yaml_test_alloc_t;

/*
 * fail_at 回目の確保で NULL を返す確保関数にする（0 なら失敗させない）。
 * 失敗させるのは fail_at 回目の 1 回だけで、それ以降の確保は成功する
 */
void mnd_yaml_test_alloc_init(mnd_yaml_test_alloc_t *ta, size_t fail_at);

/* 確保して解放していないブロックの数（allocs - frees） */
size_t mnd_yaml_test_alloc_live(const mnd_yaml_test_alloc_t *ta);

#endif
