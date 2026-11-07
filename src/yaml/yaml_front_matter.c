/* FrontMatter: 開きと閉じの行の検出、本文の開始位置 */

#include "yaml/yaml_front_matter.h"

#include <stddef.h>

/*
 * 本体はタスク 1.4 が実装する。それまでは *out をフロントマターなしにし、診断を追加せずに false を返す
 */
bool mnd_yaml_front_matter_detect(const char *data, size_t len, const char *path,
                                  mnd_yaml_front_matter_t *out, mnd_diag_list_t *diags)
{
    (void)data;
    (void)len;
    (void)path;
    (void)diags;
    out->present = false;
    out->span.start = 0;
    out->span.end = 0;
    out->content.start = 0;
    out->content.end = 0;
    out->body_start = 0;

    return false;
}
