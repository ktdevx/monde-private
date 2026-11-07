/* Flow: フロー形式のシーケンスとマッピング（明示的なスタック） */

#include "yaml/yaml_flow.h"
#include "yaml/yaml_scalar.h"

#include <stddef.h>

/*
 * 本体はタスク 2.3 が実装する。それまでは診断を追加せずに false を返す
 */
bool mnd_yaml_flow_parse(mnd_yaml_parser_t *p, mnd_yaml_cursor_t *at, mnd_yaml_node_t **out)
{
    (void)p;
    (void)at;
    (void)out;

    return false;
}
