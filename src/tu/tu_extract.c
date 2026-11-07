/* Extractor: msgid の統合、エントリの順序、#: と #. の付与 */

#include "tu/tu.h"

#include "foundation/diag.h"
#include "po/po.h"

/* 現在はエントリを作らず、out を変えずに失敗を返す */
bool mnd_tu_extract(const mnd_tu_doc_t *doc, const char *ref_file,
                    mnd_po_catalog_t *out, mnd_diag_list_t *diags)
{
    (void)doc;
    (void)ref_file;
    (void)out;
    (void)diags;
    return false;
}
