/* Parse: 入口（UTF-8 の検証、行の表の作成、改行コードの検出）とブロックのコレクションの解析 */

#include "yaml/yaml_parse.h"
#include "yaml/yaml_flow.h"
#include "yaml/yaml_internal.h"
#include "yaml/yaml_scalar.h"

#include <stddef.h>

/*
 * 本体はタスク 2.4（入口）と 2.5（ブロックのコレクション）が実装する。
 * それまでは *out を NULL にし、診断を追加せずに false を返す
 */
bool mnd_yaml_parse(const char *data, size_t len, mnd_span_t content, const mnd_yaml_options_t *options,
                    mnd_yaml_doc_t **out, mnd_diag_list_t *diags)
{
    (void)data;
    (void)len;
    (void)content;
    (void)options;
    (void)diags;
    *out = NULL;

    return false;
}
