/* update ハンドラ: .pot と既存の .po を読み、マージした .po を書く */

#include "cli/cli_internal.h"

#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>

/* 仮の実装: 診断を 1 つ追加して失敗を返す */
bool mnd_cli_cmd_update(mnd_cli_ctx_t *ctx)
{
    mnd_diag_add(ctx->diags, MND_SEVERITY_ERROR, NULL, 0, "the update command is not implemented");
    return false;
}
