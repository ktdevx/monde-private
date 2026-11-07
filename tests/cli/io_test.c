/* スイート io: -o の省略、-、- の重複、同じパスへの上書き、失敗時に出力を作らない、改行コード、BOM（2.x、3.x） */

#include <naos/naos.h>

#include "cli/cli_io.h"

#include "foundation/eol.h"

#include <stddef.h>

/* 雛形: MND_CLI_INPUT_INIT が空の入力を表す */
NAOS_TEST(io_scaffold_input_init_is_empty)
{
    mnd_cli_input_t input = MND_CLI_INPUT_INIT;

    NAOS_CHECK_NULL(input.path);
    NAOS_CHECK_UINT_EQ(0, input.content.len);
    NAOS_CHECK_FALSE(input.had_bom);
    NAOS_CHECK_INT_EQ(MND_EOL_NONE, input.eol.dominant);
    mnd_cli_input_free(&input);
}

NAOS_SUITE(io,
    NAOS_ADD_TEST(io_scaffold_input_init_is_empty)
)
