/* スイート roundtrip: spec.txt の全例のラウンドトリップ（5.1〜5.4） */

#include <naos/naos.h>

#include "../markdown/spec_reader.h"

#include "foundation/diag.h"

#include <stdbool.h>
#include <stddef.h>

/* spec.txt を MND_MD_SPEC_FILE のパスから読み、全 652 例を取り出せる */
NAOS_TEST(roundtrip_spec_file_is_readable)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_md_spec_t spec = MND_MD_SPEC_INIT;

    NAOS_REQUIRE_TRUE(mnd_md_spec_load(MND_MD_SPEC_FILE, &spec, &diags));
    NAOS_CHECK_UINT_EQ(652, spec.count);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_md_spec_free(&spec);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(roundtrip,
    NAOS_ADD_TEST(roundtrip_spec_file_is_readable)
)
