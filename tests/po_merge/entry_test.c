/* Builder: エントリの複製と、完全一致・あいまい一致・新規・廃止エントリの組み立て */

#include <naos/naos.h>

#include "po_merge/merge.h"
#include "po_merge/merge_internal.h"

NAOS_TEST(entry_scaffold_default_options_enable_fuzzy_matching)
{
    mnd_merge_options_t options = MND_MERGE_OPTIONS_INIT;

    /* 既定はあいまい一致あり（msgmerge の既定と同じ） */
    NAOS_CHECK_TRUE(options.fuzzy_matching);
}

NAOS_SUITE(entry,
    NAOS_ADD_TEST(entry_scaffold_default_options_enable_fuzzy_matching)
)
