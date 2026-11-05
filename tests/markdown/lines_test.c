/* 行ごとの内容範囲（4.x） */

#include <naos/naos.h>

#include "markdown/md_parse.h"

#include <stdbool.h>

NAOS_TEST(lines_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(lines,
    NAOS_ADD_TEST(lines_suite_is_registered)
)
