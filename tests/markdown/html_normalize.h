#ifndef MONDE_TESTS_MARKDOWN_HTML_NORMALIZE_H
#define MONDE_TESTS_MARKDOWN_HTML_NORMALIZE_H

/*
 * テスト専用の HTML の正規化（CommonMark spec 0.31.2 の test/normalize.py の移植）。
 * テストの実行ファイルにだけ含め、製品のライブラリには含めない（9.6）
 */

#include "foundation/buf.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * html[0..len)（UTF-8。NUL で終端しなくてよい）を normalize.py の normalize_html と同じ規則で
 * 正規化し、結果を out の末尾に追加する。out の既存の内容は残す。
 * 確保に失敗したら false を返す（out には途中までの結果が残りうる）。len が 0 なら html は NULL でもよい
 */
bool mnd_md_html_normalize(const char *html, size_t len, mnd_buf_t *out);

#endif
