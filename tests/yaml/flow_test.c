/* スイート flow: フロー形式のコレクション（2.3、6.2、6.4） */

#include <naos/naos.h>

#include "parser_fixture.h"
#include "yaml/yaml_flow.h"
#include "yaml/yaml_internal.h"

#include <stddef.h>

/* 雛形: カーソルは行の表の添字と文書全体を基準とするオフセットを持つ */
NAOS_TEST(flow_scaffold_cursor_shape)
{
    mnd_yaml_cursor_t at;

    at.line_index = 1;
    at.offset = 17;
    NAOS_CHECK_UINT_EQ(1, at.line_index);
    NAOS_CHECK_UINT_EQ(17, at.offset);
}

NAOS_SUITE(flow,
    NAOS_ADD_TEST(flow_scaffold_cursor_shape)
)
