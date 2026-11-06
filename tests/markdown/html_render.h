#ifndef MONDE_TESTS_MARKDOWN_HTML_RENDER_H
#define MONDE_TESTS_MARKDOWN_HTML_RENDER_H

/*
 * テスト専用の HTML 出力（commonmark.js と同じ出力規則。再帰を使わない）。
 * テストの実行ファイルにだけ含め、製品のライブラリには含めない（9.6）
 */

#include "foundation/buf.h"
#include "markdown/md.h"

#include <stdbool.h>

/*
 * doc の構文木を commonmark.js 0.31 の HTML 出力と同じ規則で HTML にし、out の末尾に追加する。
 * out の既存の内容は残す。確保に失敗したら false を返す（out には途中までの結果が残りうる）。
 * 方言の種類（MND_MD_KIND_DIALECT 以上）のノードは、子だけを出力する
 */
bool mnd_md_html_render(const mnd_md_doc_t *doc, mnd_buf_t *out);

#endif
