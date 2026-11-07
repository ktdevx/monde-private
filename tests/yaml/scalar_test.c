/* スイート scalar: スカラーの走査と論理値（2.9、3.2〜3.9、6.1、6.5） */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "parser_fixture.h"
#include "yaml/yaml_internal.h"
#include "yaml/yaml_scalar.h"

#include <stddef.h>
#include <string.h>

/* 雛形: 作業状態のエラーの報告は最初のエラーだけを診断に追加する（1 回の解析で返す診断は 1 つ） */
NAOS_TEST(scalar_scaffold_parser_reports_first_error_only)
{
    mnd_diag_list_t diags;
    mnd_yaml_parser_t p;
    const mnd_diag_t *d = NULL;

    memset(&p, 0, sizeof(p));
    mnd_diag_list_init(&diags);
    p.path = "a.md";
    p.diags = &diags;
    mnd_yaml_parser_error(&p, 3, "unterminated quoted scalar");
    NAOS_CHECK_TRUE(p.failed);
    NAOS_CHECK_FALSE(mnd_yaml_parser_claim_error(&p));
    mnd_yaml_parser_error(&p, 5, "inconsistent indentation");
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    d = mnd_diag_list_at(&diags, 0);
    NAOS_REQUIRE_TRUE(d != NULL);
    NAOS_CHECK_INT_EQ(MND_SEVERITY_ERROR, d->severity);
    NAOS_CHECK_STR_EQ("unterminated quoted scalar", d->message);
    NAOS_CHECK_STR_EQ("a.md", d->path);
    NAOS_CHECK_UINT_EQ(3, d->line);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(scalar,
    NAOS_ADD_TEST(scalar_scaffold_parser_reports_first_error_only)
)
