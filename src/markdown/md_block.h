#ifndef MONDE_MARKDOWN_MD_BLOCK_H
#define MONDE_MARKDOWN_MD_BLOCK_H

/* BlockParser: ブロックの段階の内部の宣言（md_block.c） */

#include "markdown/md_internal.h"

#include <stdbool.h>

/*
 * ブロックの段階を実行する。parser の doc（根だけを持つ文書）、line_index、空の open と leaf_lines を
 * 受け取り、本文の開始位置から内容の終わりまでを 1 行ずつ処理して、ブロックの木を文書の根の下に作る。
 * 根の範囲を [body_start, len) にし、すべてのブロックの範囲と行番号を確定する。
 * 方言のブロックの規則を新しいブロックの開始の前に登録順に試し、開いている方言のブロックの継続と閉じる処理を
 * 規則に任せる（7.2、7.3）。
 * メモリ不足か、方言の規則が不正な結果を返したら false（後者は parser の rule_failed、rule_index、rule_line に
 * 記録する。作りかけの木は文書のアリーナに残り、作業用の配列とともに Parse が解放する）
 */
bool mnd_md_block_run(mnd_md_parser_t *parser);

#endif
