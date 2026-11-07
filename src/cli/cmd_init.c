/* init ハンドラ: .pot を読み、言語を決めて .po を書く */

#include "cli/cli_internal.h"

#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>

/* 仮の実装: 診断を 1 つ追加して失敗を返す */
bool mnd_cli_cmd_init(mnd_cli_ctx_t *ctx)
{
    mnd_diag_add(ctx->diags, MND_SEVERITY_ERROR, NULL, 0, "the init command is not implemented");
    return false;
}
