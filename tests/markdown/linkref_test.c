/* 参照の解決（6.x） */

#include <naos/naos.h>

#include "markdown/md_parse.h"

#include <stdbool.h>

NAOS_TEST(linkref_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(linkref,
    NAOS_ADD_TEST(linkref_suite_is_registered)
)
