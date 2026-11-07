/* スイート report: 診断の書式、dropped、統計の書式と翻訳率の丸め、--quiet（8.x、9.1〜9.4） */

#include <naos/naos.h>

#include "cli/cli_report.h"

#include "foundation/diag.h"

/* 雛形: 空の診断の一覧はエラーを持たない（Reporter に渡す一覧の初期状態） */
NAOS_TEST(report_scaffold_empty_diag_list)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;

    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    NAOS_CHECK_FALSE(mnd_diag_list_has_error(&diags));
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(report,
    NAOS_ADD_TEST(report_scaffold_empty_diag_list)
)
