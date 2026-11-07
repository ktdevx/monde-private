/* スイート extract: ヘッダの値、--front-matter-keys、#: のファイル名、--no-wrap、空の文書、エラー（4.x） */

#include <naos/naos.h>

#include "cli/cli_commands.h"
#include "cli/cli_internal.h"

#include <stddef.h>

/* 雛形: extract のハンドラがハンドラの型で宣言されている */
NAOS_TEST(extract_scaffold_handler_is_declared)
{
    mnd_cli_handler_fn handler = mnd_cli_cmd_extract;

    NAOS_CHECK_TRUE(handler != NULL);
}

NAOS_SUITE(extract,
    NAOS_ADD_TEST(extract_scaffold_handler_is_declared)
)
