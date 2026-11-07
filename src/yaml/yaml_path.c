/* Path: キーパスの検証と照合、該当するノードの列 */

#include "yaml/yaml_path.h"
#include "yaml/yaml_internal.h"

#include <stddef.h>

/*
 * 以下の関数の本体はタスク 3.1 が実装する。それまでは *out を {NULL, 0} にし、診断を追加せずに false を返す
 */

bool mnd_yaml_path_find(const mnd_yaml_doc_t *doc, const char *path, size_t path_len,
                        mnd_yaml_matches_t *out, mnd_diag_list_t *diags)
{
    (void)doc;
    (void)path;
    (void)path_len;
    (void)diags;
    out->nodes = NULL;
    out->count = 0;

    return false;
}

void mnd_yaml_matches_free(const mnd_yaml_doc_t *doc, mnd_yaml_matches_t *matches)
{
    (void)doc;
    matches->nodes = NULL;
    matches->count = 0;
}
