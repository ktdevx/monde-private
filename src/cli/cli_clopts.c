/* Bridge: CLOpts のエラーの列を Monde の診断の列に変換する（7.4） */

#include "cli/cli_clopts.h"

#include "clopts/errors.h"
#include "foundation/diag.h"

#include <stddef.h>

void mnd_cli_clopts_errors_to_diags(const clopts_errors_t *errors, mnd_diag_list_t *diags)
{
    size_t count = clopts_errors_count(errors);
    size_t i;

    for (i = 0; i < count; i++)
    {
        /* メッセージ中の % を書式として解釈させないため、"%s" で渡す */
        mnd_diag_add(diags, MND_SEVERITY_ERROR, NULL, 0, "%s", clopts_errors_at(errors, i));
    }

    /* 記録できなかったエラーがあれば、診断の列にも記録できなかったことを残す（元の dropped は戻さない） */
    if (clopts_errors_dropped(errors))
    {
        diags->dropped = true;
    }
}
