/* html_normalize: normalize.py と同じ HTML の正規化 */

#include <naos/naos.h>

#include "html_normalize.h"

#include <stdbool.h>

NAOS_TEST(normalize_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(normalize,
    NAOS_ADD_TEST(normalize_suite_is_registered)
)
