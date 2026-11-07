#ifndef MONDE_TESTS_YAML_TREE_CHECK_H
#define MONDE_TESTS_YAML_TREE_CHECK_H

/*
 * 構文木の範囲と行番号の不変条件の検査（再帰を使わない）。各スイートが使う。
 * テストの実行ファイルにだけ含め、製品のライブラリには含めない。
 *
 * 文書の全ノードについて、次を確かめ、最初に違反したノードで止まる。
 *  1. 範囲が文書の内容の範囲（mnd_yaml_doc_content）の内側で、開始 <= 終了（3.1）。
 *  2. 子が親の範囲の内側で、子の parent が親を指す。根の parent は NULL。
 *  3. 兄弟（マッピングのキーと値の並び、シーケンスの要素）の範囲が重ならず原文の順に並ぶ。
 *  4. start_line と end_line が span.start と span.end の行番号（foundation の mnd_source_line_of の数え方）。
 * 空のドキュメント（根が NULL）は満たすものとする
 */

#include "yaml/yaml.h"

#include <stdbool.h>
#include <stddef.h>

#define MND_YAML_TREE_CHECK_MESSAGE_MAX 256

typedef struct mnd_yaml_tree_check_result
{
    bool ok;
    const mnd_yaml_node_t *node;  /* 最初に違反したノード（ok か、文書全体の誤りなら NULL） */
    char message[MND_YAML_TREE_CHECK_MESSAGE_MAX]; /* 違反の説明（ok なら空文字列） */
} mnd_yaml_tree_check_result_t;

/*
 * doc の構文木の不変条件を確かめ、結果を *result に入れる（result は NULL 可）。
 * すべて満たせば true。違反があるか、doc が NULL か、作業用のメモリが確保できなければ false
 */
bool mnd_yaml_tree_check(const mnd_yaml_doc_t *doc, mnd_yaml_tree_check_result_t *result);

/*
 * mnd_yaml_tree_check を呼び、違反があれば説明を NAOS_NOTE で表示する。結果を返す。
 * テストでは NAOS_CHECK_TRUE(mnd_yaml_tree_check_note(doc)) のように使う
 */
bool mnd_yaml_tree_check_note(const mnd_yaml_doc_t *doc);

#endif
