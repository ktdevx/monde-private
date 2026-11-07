#ifndef MONDE_YAML_YAML_SCALAR_H
#define MONDE_YAML_YAML_SCALAR_H

/*
 * スカラーの走査と論理値（Scalar）の内部ヘッダ。src/yaml/ の .c と tests/yaml/ だけが include する。
 * 解析の作業状態の補助関数（mnd_yaml_parser_error など。yaml_internal.h）も yaml_scalar.c に置く
 */

#include "yaml/yaml_internal.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * at の位置から 1 つのスカラーを読み、ノードを作って *out に設定し、*at を終わりの直後に進める。
 * プレーン・シングルクオート・ダブルクオート・ブロックスカラー（| と >）を扱う。
 * ブロックスカラーでは、*at を消費した最後の行の次の行の先頭に進める（line_index も進める）。
 * parent_indent はブロックスカラーと継続行の判定に使う親のインデント。flow はフロー文脈
 * （作ったスカラーの flow も true にする）。
 * ノードの先頭の &、*、! は非対応の構文、@ と ` は予約の指示文字のエラーにする。
 * 失敗（構文エラー、メモリ不足）なら p の診断に追加して false（*out と *at は変更しない）
 */
bool mnd_yaml_scalar_parse(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, size_t parent_indent, bool flow,
                           mnd_yaml_node_t **out);

/* at がブロックスカラーのヘッダ（| か >）で始まるか */
bool mnd_yaml_scalar_is_block_header(const mnd_yaml_parser_t *p, mnd_yaml_cursor_t at);

#endif
