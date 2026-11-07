/* スイート init: Language と PO-Revision-Date、fuzzy の除去、読み込みエラー（5.1、5.9〜5.10） */

#include <naos/naos.h>

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"

#include <stddef.h>

/* 雛形: init のハンドラがハンドラの型で宣言されている */
NAOS_TEST(init_scaffold_handler_is_declared)
{
    mnd_cli_handler_fn handler = mnd_cli_cmd_init;

    NAOS_CHECK_TRUE(handler != NULL);
}

NAOS_SUITE(init,
    NAOS_ADD_TEST(init_scaffold_handler_is_declared)
)
