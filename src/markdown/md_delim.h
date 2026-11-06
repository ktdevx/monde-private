#ifndef MONDE_MARKDOWN_MD_DELIM_H
#define MONDE_MARKDOWN_MD_DELIM_H

/* Delims: 区切り記号の連なりの判定と強調の処理の内部の宣言（md_delim.c） */

#include "markdown/md.h"
#include "markdown/md_dialect.h"
#include "markdown/md_internal.h"

#include <stdbool.h>
#include <stddef.h>

/* 区切り記号のスタックの要素の添字がないことを表す値 */
#define MND_MD_DELIM_NONE ((size_t)-1)

/*
 * 区切り記号のスタックの要素（spec の付録の delimiter stack）。スタックは要素の大きさが
 * sizeof(mnd_md_delim_t) の作業用の配列で、積んだ順（原文の順）に並ぶ。強調の処理の途中で取り除いた要素は、
 * prev と next の連結から外す（配列には残る）。処理の最後に、下限より上の要素を配列から取り除く
 */
typedef struct mnd_md_delim
{
    mnd_md_node_t *node;        /* 区切り記号の連なりを表すテキストのノード（残っている記号の範囲） */
    unsigned char ch;           /* '*'、'_'、または方言の区切り記号形式の規則の開始文字 */
    mnd_md_delim_run_t run;     /* 残っている数、元の長さ、開きと閉じの可否 */
    size_t prev;                /* 連結の前の要素の添字。なければ MND_MD_DELIM_NONE */
    size_t next;                /* 連結の後ろの要素の添字。なければ MND_MD_DELIM_NONE */
} mnd_md_delim_t;

/*
 * s[0..len) の中の区切り記号の連なり s[start..end)（start < end）の開きと閉じの可否を判定する
 * （spec の left-flanking と right-flanking）。前後の文字は UTF-8 として読み、s の先頭より前と終わりより後ろは
 * 空白とみなす。空白は Unicode の Zs とタブ・LF・FF・CR、句読点は一般カテゴリ P と S。
 * intraword が真なら `_` の規則（単語の中では開きも閉じもしない）、偽なら `*` の規則を使う
 */
void mnd_md_delim_classify(const char *s, size_t len, size_t start, size_t end, bool intraword,
                           bool *can_open, bool *can_close);

/*
 * 区切り記号のスタック stack（mnd_md_delim_t の配列）の末尾に要素を積む。node は連なりのテキストのノード、
 * length は連なりの長さ（1 以上）。メモリ不足なら false（スタックは変更しない）
 */
bool mnd_md_delim_push(mnd_md_array_t *stack, mnd_md_node_t *node, unsigned char ch, size_t length,
                       bool can_open, bool can_close);

/*
 * spec の process emphasis。スタックの添字 stack_bottom 以上の要素（下限より上の要素）を処理し、
 * 対になった開きと閉じの間の兄弟を、強調（MND_MD_EMPH）または強い強調（MND_MD_STRONG）のノードの子にする。
 * 開きは残っている記号の後ろから、閉じは前から使い、テキストのノードの範囲を縮める。記号が残らない
 * テキストのノードは木から外す。強調のノードの範囲は使った開きの記号の先頭から使った閉じの記号の後ろまでで、
 * 開きと閉じの区切り記号の範囲（as.emph）と行番号を記録する。3 の倍数の規則と、区切り記号の文字・
 * 閉じが開きにもなれるか・元の長さを 3 で割った余りの組ごとの探索の下限（openers_bottom）で、
 * 要素の数に比例する時間で終わる（8.3）。
 * 方言の区切り記号の文字（parser->delim_slot が 0 でない文字）の閉じは、同じ文字の開きとの対を、その文字の
 * 区切り記号形式の規則の match_delims で登録順に判定し（3 の倍数の規則は適用しない）、最初に真を返した規則の
 * 種類のノードを作る。ノードの範囲と記号の使い方は強調と同じで、mnd_md_emph_t と同じ形のデータ（開きと閉じの
 * 範囲）をアリーナに確保して as.dialect_data に入れる。探索の下限の組は強調と同じ形（文字ごとに、閉じが開きにも
 * なれるか × 元の長さを 3 で割った余り）である。
 * 最後に、stack_bottom 以上の要素をスタックから取り除く（リンクを閉じるとき（4.3）は、角括弧を積んだときの
 * スタックの要素数を stack_bottom にして呼ぶ）。メモリ不足なら false（作りかけのノードは文書のアリーナに残る）。
 * 規則が不正な数（0、または開きか閉じの残りの数を超える数）を返したら、parser の rule_failed、rule_index
 * （インラインの規則の配列の添字）、rule_line（閉じの行）を記録して false
 */
bool mnd_md_delim_process(mnd_md_parser_t *parser, mnd_md_array_t *stack, size_t stack_bottom);

#endif
