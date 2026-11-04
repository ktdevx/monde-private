#ifndef MONDE_TESTS_MARKDOWN_SPEC_READER_H
#define MONDE_TESTS_MARKDOWN_SPEC_READER_H

/*
 * spec.txt から例（番号、節、Markdown、期待 HTML）を取り出す（spec_tests.py と同じ規則）。
 * テストの実行ファイルにだけ含め、製品のライブラリには含めない（9.6）
 */

#include <stdbool.h>
#include <stddef.h>

#include "foundation/buf.h"
#include "foundation/diag.h"

/* spec.txt の 1 つの例。文字列はすべて NUL 終端で、data は NULL にならない */
typedef struct mnd_md_spec_example
{
    size_t number;      /* 1 起点の例の番号（spec_tests.py の example） */
    size_t start_line;  /* 開きの行の 1 起点の行番号（Markdown が空なら 0。spec_tests.py の start_line） */
    size_t end_line;    /* 閉じの行の 1 起点の行番号（spec_tests.py の end_line） */
    mnd_buf_t section;  /* 例の前の最後の見出しの文字列（spec_tests.py の section） */
    mnd_buf_t markdown; /* 例の Markdown（`→` はタブに置き換え済み） */
    mnd_buf_t html;     /* 期待 HTML（`→` はタブに置き換え済み） */
} mnd_md_spec_example_t;

/* 取り出した例の並び。examples[i].number == i + 1 */
typedef struct mnd_md_spec
{
    mnd_md_spec_example_t *examples;
    size_t count;
    size_t cap;
} mnd_md_spec_t;

#define MND_MD_SPEC_INIT { NULL, 0, 0 }

void mnd_md_spec_init(mnd_md_spec_t *spec);

/*
 * text の len バイトを spec.txt の内容として例を取り出し、spec に入れる（既存の例は解放する）。
 * 行は LF だけで区切る。NUL を含んでもよい。
 * 成功: true。メモリ不足: false で、spec は空。
 */
bool mnd_md_spec_parse(const char *text, size_t len, mnd_md_spec_t *spec);

/*
 * path のファイルを foundation の mnd_file_read で読み、mnd_md_spec_parse で例を取り出す。
 * 失敗: diags にエラーを 1 つ以上追加して false で、spec は空。
 */
bool mnd_md_spec_load(const char *path, mnd_md_spec_t *spec, mnd_diag_list_t *diags);

/* すべての例を解放し、初期状態に戻す。初期状態・解放済みに対しても安全 */
void mnd_md_spec_free(mnd_md_spec_t *spec);

#endif
