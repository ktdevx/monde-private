#ifndef MONDE_TESTS_MARKDOWN_TREE_CHECK_H
#define MONDE_TESTS_MARKDOWN_TREE_CHECK_H

/*
 * 構文木の範囲の不変条件の検査（再帰を使わない）。spec のテストと個別のテストで使う。
 * テストの実行ファイルにだけ含め、製品のライブラリには含めない。
 *
 * 文書を前順にたどり（mnd_md_node_next）、最初に違反したノードで止まる。各ノードについて、次の順に確かめる。
 *  1. 範囲が内容の範囲内で、開始 <= 終了（3.1）。根は MND_MD_DOCUMENT で、範囲の終わりが内容の長さ。
 *  2. 親・兄弟の連結が矛盾しない（prev->next == node など）。
 *  3. 子が親の範囲の内側（3.2）。
 *  4. 直前の兄弟の範囲の終わり <= 自分の範囲の開始（重ならず原文の順。空の範囲は同じ位置に並んでよい）（3.3）。
 *  5. start_line が span.start の行番号、end_line が範囲の最後のバイト（範囲が空なら span.start）の行番号
 *     （foundation の mnd_source_line_of の数え方）。
 *  6. lines を持つノードの各行について（4.1〜4.3）:
 *     - prefix.start <= prefix.end == content.start <= content.end
 *     - content.start を含む物理的な行（line_start <= content.start <= line_end）があり、
 *       prefix.start がその行の line_start（プレフィックスが物理的な行の先頭から始まる）
 *     - content.end がその行の line_end 以下（内容が行末記号を含まず、次の行にまたがらない）
 *     - eol_len がその物理的な行の行末記号のバイト数と一致し、pad が 3 以下
 *     - line が content.start の行番号
 *     - 内容がノードの範囲の内側
 *     - 2 行目以降は、前の行より後ろの物理的な行（prefix.start が増え、前の行の content.end <= prefix.start）
 *  ATX 見出しの行の内容は閉じの # の前で終わるので、content.end == line_end は求めない。
 *
 * 物理的な行は、根の範囲の開始（本文の開始位置）から内容の終わりまでを mnd_md_lines_next で分割して求める
 * （解析と同じ分割。本文の開始位置は物理的な行の先頭として扱う）。
 */

#include "markdown/md.h"

#include <stdbool.h>
#include <stddef.h>

/* 違反が行に関するものでないときの line_index */
#define MND_MD_TREE_CHECK_NO_LINE ((size_t)-1)

#define MND_MD_TREE_CHECK_MESSAGE_MAX 256

typedef struct mnd_md_tree_check_result
{
    bool ok;
    const mnd_md_node_t *node;  /* 最初に違反したノード（ok か、文書全体の誤りなら NULL） */
    size_t line_index;          /* node->lines の中の違反した行。行の違反でなければ MND_MD_TREE_CHECK_NO_LINE */
    char message[MND_MD_TREE_CHECK_MESSAGE_MAX]; /* 違反の説明（ok なら空文字列） */
} mnd_md_tree_check_result_t;

/*
 * doc の構文木の不変条件を確かめ、結果を *result に入れる（result は NULL 可）。
 * すべて満たせば true。違反があるか、doc が NULL か、作業用のメモリが確保できなければ false
 */
bool mnd_md_tree_check(const mnd_md_doc_t *doc, mnd_md_tree_check_result_t *result);

/*
 * mnd_md_tree_check を呼び、違反があれば説明を NAOS_NOTE で表示する。結果を返す。
 * テストでは NAOS_CHECK_TRUE(mnd_md_tree_check_note(doc)) のように使う
 */
bool mnd_md_tree_check_note(const mnd_md_doc_t *doc);

#endif
