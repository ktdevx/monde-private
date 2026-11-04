/* Builder: エントリの複製と、完全一致・あいまい一致・新規・廃止エントリの組み立て */

#include "po_merge/merge_internal.h"

bool mnd_merge_has_translation(const mnd_po_entry_t *entry)
{
    return (entry->msgstr.count >= 1) && (entry->msgstr.items[0].len > 0);
}
