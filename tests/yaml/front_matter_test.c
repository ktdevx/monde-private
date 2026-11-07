/* スイート front_matter: フロントマターの検出（1.1〜1.7、6.6、7.1） */

#include <naos/naos.h>

#include "yaml/yaml_front_matter.h"

#include <stdbool.h>
#include <stddef.h>

/* 雛形: フロントマターがない結果の形 {false, {0,0}, {0,0}, 0} を表せる */
NAOS_TEST(front_matter_scaffold_absent_result_shape)
{
    mnd_yaml_front_matter_t fm = { false, { 0, 0 }, { 0, 0 }, 0 };

    NAOS_CHECK_FALSE(fm.present);
    NAOS_CHECK_UINT_EQ(0, fm.span.start);
    NAOS_CHECK_UINT_EQ(0, fm.span.end);
    NAOS_CHECK_UINT_EQ(0, fm.content.start);
    NAOS_CHECK_UINT_EQ(0, fm.content.end);
    NAOS_CHECK_UINT_EQ(0, fm.body_start);
}

NAOS_SUITE(front_matter,
    NAOS_ADD_TEST(front_matter_scaffold_absent_result_shape)
)
