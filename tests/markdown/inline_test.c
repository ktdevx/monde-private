/* インラインの種類と範囲（5.x） */

#include <naos/naos.h>

#include "markdown/md_parse.h"

#include <stdbool.h>

NAOS_TEST(inline_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(inline,
    NAOS_ADD_TEST(inline_suite_is_registered)
)
