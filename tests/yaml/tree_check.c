/* tree_check: 構文木の範囲と行番号の不変条件の検査 */

#include "tree_check.h"

#include <stddef.h>

/*
 * 検査の本体はタスク 1.2 が実装する。それまでは検査を行わず、満たさないもの（false）として返す
 */
bool mnd_yaml_tree_check(const mnd_yaml_doc_t *doc, mnd_yaml_tree_check_result_t *result)
{
    (void)doc;
    if (result != NULL)
    {
        result->ok = false;
        result->node = NULL;
        result->message[0] = '\0';
    }

    return false;
}

bool mnd_yaml_tree_check_note(const mnd_yaml_doc_t *doc)
{
    return mnd_yaml_tree_check(doc, NULL);
}
