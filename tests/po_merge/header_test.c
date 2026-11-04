/* Header: ヘッダのマージと nplurals の読み取り */

#include <naos/naos.h>

#include "po_merge/merge_internal.h"

NAOS_TEST(header_scaffold_nplurals_limits)
{
    /* Plural-Forms から読めない、または 1〜100 の範囲外なら 2 */
    NAOS_CHECK_UINT_EQ(MND_MERGE_NPLURALS_DEFAULT, 2);
    NAOS_CHECK_UINT_EQ(MND_MERGE_NPLURALS_MAX, 100);
}

NAOS_SUITE(header,
    NAOS_ADD_TEST(header_scaffold_nplurals_limits)
)
