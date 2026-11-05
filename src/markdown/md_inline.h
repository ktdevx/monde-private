#ifndef MONDE_MARKDOWN_MD_INLINE_H
#define MONDE_MARKDOWN_MD_INLINE_H

/* InlineParser: インラインの段階の内部の宣言（md_inline.c） */

#include "markdown/md_internal.h"

#include <stdbool.h>

/*
 * インラインの段階を実行する。ブロックの段階が終わった parser の文書（ブロックの木、行、定義の表が確定済み）を
 * 前順に再帰なしでたどり、インラインの対象のリーフ（段落と見出し）ごとに、行の内容をインラインのノードに分けて
 * そのリーフの子にする（5.1、5.11）。すべての範囲は内容全体の先頭を基準とする原文のオフセットで、
 * 行番号も確定する。作業用の配列は parser の確保関数から確保し、成功・失敗のどちらでも解放してから戻る。
 * メモリ不足なら false（作りかけのノードは文書のアリーナに残り、Parse が文書とともに解放する）
 */
bool mnd_md_inline_run(mnd_md_parser_t *parser);

#endif
