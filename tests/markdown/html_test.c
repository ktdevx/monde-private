/* Html: HTML の字句と HTML ブロックの条件 */

#include <naos/naos.h>

#include "markdown/md_html.h"

#include <stdbool.h>

NAOS_TEST(html_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(html,
    NAOS_ADD_TEST(html_suite_is_registered)
)
