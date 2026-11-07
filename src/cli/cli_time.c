/* Clock: 現在日時と UTC オフセットから PO の日時文字列を作る */

#include "cli/cli_time.h"

#include "foundation/diag.h"

#include <stdbool.h>

/* 仮の実装: 診断を 1 つ追加して失敗を返す */
bool mnd_cli_format_now(const mnd_cli_env_t *env, char out[MND_PO_DATE_SIZE], mnd_diag_list_t *diags)
{
    (void)env;
    out[0] = '\0';
    mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "formatting the current time is not implemented");
    return false;
}
