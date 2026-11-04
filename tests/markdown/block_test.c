/* ブロックの種類と属性（2.x） */

#include <naos/naos.h>

#include "markdown/md_parse.h"

#include <stdbool.h>

NAOS_TEST(block_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(block,
    NAOS_ADD_TEST(block_suite_is_registered)
)
