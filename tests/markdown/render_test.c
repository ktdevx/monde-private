/* html_render: commonmark.js と同じ規則の HTML 出力 */

#include <naos/naos.h>

#include "html_render.h"

#include <stdbool.h>

NAOS_TEST(render_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(render,
    NAOS_ADD_TEST(render_suite_is_registered)
)
