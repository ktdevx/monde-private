/* スイート time: 固定時刻からの日時文字列、正負のオフセット、日付境界（4.2、5.8） */

#include <naos/naos.h>

#include "cli/cli_time.h"

#include <string.h>

/* 雛形: "YYYY-MM-DD HH:MM+ZZZZ" と終端の NUL が MND_PO_DATE_SIZE に収まる */
NAOS_TEST(time_scaffold_date_size_fits_format)
{
    NAOS_CHECK_UINT_LT(strlen("2026-01-01 00:00+0000"), MND_PO_DATE_SIZE);
}

NAOS_SUITE(time,
    NAOS_ADD_TEST(time_scaffold_date_size_fits_format)
)
