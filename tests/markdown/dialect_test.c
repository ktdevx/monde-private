/* テスト用の方言による拡張点（7.x） */

#include <naos/naos.h>

#include "markdown/md_dialect.h"
#include "markdown/md_parse.h"

#include <stdbool.h>

NAOS_TEST(dialect_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(dialect,
    NAOS_ADD_TEST(dialect_suite_is_registered)
)
