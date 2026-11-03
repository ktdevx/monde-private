#include <naos/naos.h>

#include "foundation/buf.h"
#include "foundation/diag.h"
#include "foundation/file_io.h"
#include "po/po_read.h"
#include "po/po_write.h"

NAOS_TEST(roundtrip_test_data_dir_is_readable)
{
    mnd_buf_t content = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool had_bom = true;

    NAOS_CHECK_TRUE(mnd_file_read(MND_PO_TEST_DATA_DIR "/README.md", &content, &had_bom, &diags));
    NAOS_CHECK_UINT_GT(content.len, 0);
    NAOS_CHECK_FALSE(had_bom);
    NAOS_CHECK_UINT_EQ(0, mnd_diag_list_count(&diags));
    mnd_buf_free(&content);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(roundtrip,
    NAOS_ADD_TEST(roundtrip_test_data_dir_is_readable)
)
