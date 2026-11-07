/* スイート translate: 置き換え、--use-fuzzy、--front-matter-keys、警告、エラー、廃止・msgctxt の無視（7.x） */

#include <naos/naos.h>

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"

#include <stddef.h>

/* 雛形: translate のハンドラがハンドラの型で宣言されている */
NAOS_TEST(translate_scaffold_handler_is_declared)
{
    mnd_cli_handler_fn handler = mnd_cli_cmd_translate;

    NAOS_CHECK_TRUE(handler != NULL);
}

NAOS_SUITE(translate,
    NAOS_ADD_TEST(translate_scaffold_handler_is_declared)
)
