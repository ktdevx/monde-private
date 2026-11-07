#ifndef MONDE_YAML_YAML_FORMAT_H
#define MONDE_YAML_YAML_FORMAT_H

/* 訳文の整形（Formatter）の公開ヘッダ */

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "yaml/yaml.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * scalar の範囲を置き換えるバイト列を out の後ろに追加して true を返す。
 * text[0..len) は訳文の論理値（UTF-8、改行は LF、NUL を含まない）。
 * 不正な UTF-8、NUL、メモリ不足は diags にエラーを追加して false を返す
 * （失敗時は out にそれまでに追加した内容が残る）。
 * path は診断に付けるパス（NULL 可）。
 * 事前条件: scalar->kind == MND_YAML_SCALAR で、doc のノードであること。
 * 事後条件: 元の内容の scalar->span を出力で置き換えて解析し直すと、同じ位置のスカラーの論理値が text と一致する
 */
bool mnd_yaml_format_value(const mnd_yaml_doc_t *doc, const mnd_yaml_node_t *scalar,
                           const char *text, size_t len, const char *path,
                           mnd_buf_t *out, mnd_diag_list_t *diags);

#endif
