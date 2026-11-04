/* Scan: 1 行の中のブロックの開始の判定とタブの桁 */

#include <naos/naos.h>

#include "markdown/md_scan.h"

#include <stdbool.h>

NAOS_TEST(scan_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(scan,
    NAOS_ADD_TEST(scan_suite_is_registered)
)
