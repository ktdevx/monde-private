/* ソース範囲と行番号（3.x） */

#include <naos/naos.h>

#include "markdown/md_parse.h"

#include <stdbool.h>

NAOS_TEST(range_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(range,
    NAOS_ADD_TEST(range_suite_is_registered)
)
