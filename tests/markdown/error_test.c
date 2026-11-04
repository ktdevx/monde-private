/* 不正な UTF-8、メモリ不足、病的な入力（8.x） */

#include <naos/naos.h>

#include "markdown/md_parse.h"
#include "test_alloc.h"

#include <stdbool.h>

NAOS_TEST(error_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(error,
    NAOS_ADD_TEST(error_suite_is_registered)
)
