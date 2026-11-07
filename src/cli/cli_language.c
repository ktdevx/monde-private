/* Language: --language と環境変数からの言語コードの決定と検査 */

#include "cli/cli_language.h"

#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>

/* 仮の実装: 診断を 1 つ追加して失敗を返す */
bool mnd_cli_resolve_language(const mnd_cli_env_t *env, const char *explicit,
                              mnd_cli_language_t *out, mnd_diag_list_t *diags)
{
    (void)env;
    (void)explicit;
    out->code = NULL;
    out->variable = NULL;
    mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "resolving the language is not implemented");
    return false;
}
