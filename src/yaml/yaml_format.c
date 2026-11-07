/* Formatter: スタイルの維持の判定、各スタイルの整形、ダブルクオートへの切り替え */

#include "yaml/yaml_format.h"
#include "yaml/yaml_internal.h"

#include <stddef.h>

/*
 * 本体はタスク 3.2（プレーンと引用符付き）と 3.3（ブロックスカラー）が実装する。
 * それまでは out に何も追加せず、診断を追加せずに false を返す
 */
bool mnd_yaml_format_value(const mnd_yaml_doc_t *doc, const mnd_yaml_node_t *scalar,
                           const char *text, size_t len, const char *path,
                           mnd_buf_t *out, mnd_diag_list_t *diags)
{
    (void)doc;
    (void)scalar;
    (void)text;
    (void)len;
    (void)path;
    (void)out;
    (void)diags;

    return false;
}
