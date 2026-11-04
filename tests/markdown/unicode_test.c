/* Unicode: 一般カテゴリの判定と大文字小文字の畳み込み */

#include <naos/naos.h>

#include "markdown/md_unicode.h"

#include <stdbool.h>

NAOS_TEST(unicode_suite_is_registered)
{
    /* 雛形のテスト。このスイートのテストは、担当するタスクがこのファイルに書く */
    NAOS_CHECK_TRUE(true);
}

NAOS_SUITE(unicode,
    NAOS_ADD_TEST(unicode_suite_is_registered)
)
