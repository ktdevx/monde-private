#ifndef MONDE_YAML_YAML_PATH_H
#define MONDE_YAML_YAML_PATH_H

/* キーパスによる照合（Path）の公開ヘッダ */

#include "foundation/diag.h"
#include "yaml/yaml.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct mnd_yaml_matches
{
    const mnd_yaml_node_t **nodes;  /* 原文の順。スカラー以外も含む（kind で区別する） */
    size_t count;
} mnd_yaml_matches_t;

/*
 * path[0..path_len) のキーパスに当たるノードを *out に入れて true を返す。該当なしは count == 0 で true。
 * 不正なキーパスとメモリ不足は diags にエラーを追加し、*out を {NULL, 0} にして false を返す。
 * 結果の配列は文書の確保関数で確保し、mnd_yaml_matches_free で解放する
 */
bool mnd_yaml_path_find(const mnd_yaml_doc_t *doc, const char *path, size_t path_len,
                        mnd_yaml_matches_t *out, mnd_diag_list_t *diags);

/* 結果の配列を解放し {NULL, 0} にする。{NULL, 0} に対しても安全 */
void mnd_yaml_matches_free(const mnd_yaml_doc_t *doc, mnd_yaml_matches_t *matches);

#endif
