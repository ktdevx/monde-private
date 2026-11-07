/* スイート error: メモリ不足、病的な入力、診断の扱い（6.1、6.6〜6.10） */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "test_alloc.h"
#include "yaml/yaml_internal.h"

#include <stddef.h>
#include <string.h>

/* 雛形: メモリ不足は行番号 0 の "out of memory" のエラーとして報告される */
NAOS_TEST(error_scaffold_parser_oom_message)
{
    mnd_diag_list_t diags;
    mnd_yaml_parser_t p;
    const mnd_diag_t *d = NULL;

    memset(&p, 0, sizeof(p));
    mnd_diag_list_init(&diags);
    p.diags = &diags;
    mnd_yaml_parser_oom(&p);
    NAOS_CHECK_TRUE(p.failed);
    NAOS_REQUIRE_UINT_EQ(1, mnd_diag_list_count(&diags));
    d = mnd_diag_list_at(&diags, 0);
    NAOS_REQUIRE_TRUE(d != NULL);
    NAOS_CHECK_STR_EQ("out of memory", d->message);
    NAOS_CHECK_NULL(d->path);
    NAOS_CHECK_UINT_EQ(0, d->line);
    NAOS_CHECK_TRUE(mnd_diag_list_has_error(&diags));
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(error,
    NAOS_ADD_TEST(error_scaffold_parser_oom_message)
)
