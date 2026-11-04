/* Model: 構文木の型、アリーナ、走査、解放と、テスト用の確保関数 */

#include <naos/naos.h>

#include "markdown/md.h"
#include "test_alloc.h"

NAOS_TEST(model_kind_dialect_starts_after_commonmark_kinds)
{
    NAOS_CHECK_INT_EQ(MND_MD_KIND_DIALECT, 0x100);
    NAOS_CHECK_INT_GT(MND_MD_KIND_DIALECT, MND_MD_HTML_INLINE);
}

NAOS_SUITE(model,
    NAOS_ADD_TEST(model_kind_dialect_starts_after_commonmark_kinds)
)
