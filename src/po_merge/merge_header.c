/* Header: ヘッダの msgstr のマージ、ヘッダのエントリの組み立て、nplurals の読み取り */

#include "po_merge/merge_internal.h"

/*
 * 仮の実装: ヘッダの規則を実装するまで、dst を変えずに false を返す
 */
bool mnd_merge_header(mnd_po_entry_t *dst, const mnd_po_entry_t *old_header,
                      const mnd_po_entry_t *template_header)
{
    (void)dst;
    (void)old_header;
    (void)template_header;

    return false;
}
