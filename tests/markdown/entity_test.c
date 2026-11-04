/* Entity: 実体名の表と検索 */

#include <naos/naos.h>

#include "markdown/md_entity.h"

#include <stdbool.h>

NAOS_TEST(entity_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(entity,
    NAOS_ADD_TEST(entity_suite_is_registered)
)
