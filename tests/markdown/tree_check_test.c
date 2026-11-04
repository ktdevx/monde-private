/* tree_check: 範囲の不変条件の検査そのもののテスト */

#include <naos/naos.h>

#include "tree_check.h"

#include <stdbool.h>

NAOS_TEST(tree_check_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(tree_check,
    NAOS_ADD_TEST(tree_check_suite_is_registered)
)
