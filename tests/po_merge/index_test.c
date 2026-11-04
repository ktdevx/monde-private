/* Hash と KeyIndex: バイト列キーのハッシュ表、msgctxt と msgid による索引 */

#include <naos/naos.h>

#include "po_merge/merge_internal.h"

NAOS_TEST(index_scaffold_key_separator_is_eot)
{
    /* キーは「msgctxt の有無の 1 バイト + msgctxt + 0x04 + msgid」の連結 */
    NAOS_CHECK_UINT_EQ((unsigned char)MND_MERGE_KEY_SEPARATOR, 0x04u);
}

NAOS_SUITE(index,
    NAOS_ADD_TEST(index_scaffold_key_separator_is_eot)
)
