/* LineIndex: 行の分割と行番号 */

#include <naos/naos.h>

#include "markdown/md_lines.h"

#include <stdbool.h>

NAOS_TEST(lines_index_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(lines_index,
    NAOS_ADD_TEST(lines_index_suite_is_registered)
)
