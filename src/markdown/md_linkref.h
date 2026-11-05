#ifndef MONDE_MARKDOWN_MD_LINKREF_H
#define MONDE_MARKDOWN_MD_LINKREF_H

/* LinkRef: リンク参照定義の解析、ラベルの正規化、定義の表の内部の宣言（md_linkref.c） */

#include "markdown/md.h"
#include "markdown/md_internal.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * リンクラベルの角括弧の内側の最大の文字数（コードポイントの数。行の境目は 1 文字）。
 * インラインの段階（リンクのラベル）も同じ上限を使う
 */
#define MND_MD_LINKREF_LABEL_MAX 999

/*
 * ラベルの論理的な文字列 text[0..len)（角括弧の内側。行の境目は "\n"）を正規化し、
 * その UTF-8 の列を out（要素の大きさが 1 の作業用の配列）の後ろに追加する（6.2）。
 * 前後の空白を除き、連続する空白（Unicode の空白: Zs、タブ、LF、FF、CR）を 1 つの空白にまとめ、
 * 大文字小文字の畳み込み（CaseFolding の C と F）を行う。エスケープは文字に戻さない。
 * NUL と不正な UTF-8 のバイトは U+FFFD として扱う（結果は NUL を含まない）。out は NUL で終端しない。
 * メモリ不足なら false（out にそれまでに追加した内容は残る）
 */
bool mnd_md_linkref_normalize(const char *text, size_t len, mnd_md_array_t *out);

/*
 * 定義の表から、正規化したラベル norm[0..len) の定義のノードを引く。同じラベルの定義が複数あれば、
 * 文書で最初に現れたもの（6.3）。見つからなければ NULL。ブロックの段階が終わったあとは、
 * 表には文書のすべての定義が入っている（6.1）
 */
const mnd_md_node_t *mnd_md_linkref_lookup(const mnd_md_doc_t *doc, const char *norm, size_t len);

/*
 * 段落 paragraph の行 lines[0..count)（段落の先頭から）から、リンク参照定義を繰り返し取り出す（2.7、2.8）。
 * 定義ごとに、その定義を構成する行を持つ MND_MD_LINK_REF_DEF のノードを作って paragraph の直前の兄弟に置き、
 * 範囲と行番号を確定し、正規化したラベルで定義の表に登録する（同じラベルが登録済みなら登録しない）。
 * 定義に使った行の数を *taken に入れる（定義がなければ 0）。paragraph の範囲と行は変更しない。
 * メモリ不足なら false
 */
bool mnd_md_linkref_take(mnd_md_parser_t *parser, mnd_md_node_t *paragraph, const mnd_md_line_t *lines,
                         size_t count, size_t *taken);

#endif
