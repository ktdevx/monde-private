#ifndef MONDE_YAML_YAML_FLOW_H
#define MONDE_YAML_YAML_FLOW_H

/*
 * フロー形式のコレクション（Flow）の内部ヘッダ。src/yaml/ の .c と tests/yaml/ だけが include する。
 * 入れ子は明示的なスタックで扱い、再帰を使わない
 */

#include "yaml/yaml_internal.h"

#include <stdbool.h>

/*
 * at が [ または { で始まるフロー形式のコレクションを読み、*out に設定し、*at を閉じ括弧の直後に進める
 * （複数の行にまたがれば line_index も進める）。
 * 失敗（構文エラー、同じキー、メモリ不足）なら p の診断に追加して false（*out と *at は変更しない）
 */
bool mnd_yaml_flow_parse(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, mnd_yaml_node_t **out);

#endif
