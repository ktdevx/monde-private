/* Text: 論理的な文字列とアンエスケープ */

#include <naos/naos.h>

#include "markdown/md.h"

#include <stdbool.h>

NAOS_TEST(text_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(text,
    NAOS_ADD_TEST(text_suite_is_registered)
)
