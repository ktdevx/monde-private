/* スイート update: 引き継ぎ、--no-fuzzy-matching、ヘッダの保持、同じパスへの上書き、エラー（6.x） */

#include <naos/naos.h>

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"

#include <stddef.h>

/* 雛形: update のハンドラがハンドラの型で宣言されている */
NAOS_TEST(update_scaffold_handler_is_declared)
{
    mnd_cli_handler_fn handler = mnd_cli_cmd_update;

    NAOS_CHECK_TRUE(handler != NULL);
}

NAOS_SUITE(update,
    NAOS_ADD_TEST(update_scaffold_handler_is_declared)
)
