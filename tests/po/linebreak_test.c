#include <naos/naos.h>

#include "po/po_internal.h"

NAOS_TEST(linebreak_pair_table_classes_come_first)
{
    /* ペアテーブルの 32 クラス（OP〜ZWJ）が先に並び、解決前だけのクラスがその後ろに続く */
    NAOS_CHECK_INT_EQ(0, MND_PO_LB_OP);
    NAOS_CHECK_INT_EQ(31, MND_PO_LB_ZWJ);
    NAOS_CHECK_INT_EQ(32, MND_PO_LB_SP);
    NAOS_CHECK_INT_EQ(41, MND_PO_LB_XX);
}

NAOS_SUITE(linebreak,
    NAOS_ADD_TEST(linebreak_pair_table_classes_come_first)
)
