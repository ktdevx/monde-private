/* extract ハンドラ: .md を読み、ヘッダ付きの翻訳テンプレートを書く */

#include "cli/cli_internal.h"

#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>

/* 仮の実装: 診断を 1 つ追加して失敗を返す */
bool mnd_cli_cmd_extract(mnd_cli_ctx_t *ctx)
{
    mnd_diag_add(ctx->diags, MND_SEVERITY_ERROR, NULL, 0, "the extract command is not implemented");
    return false;
}
