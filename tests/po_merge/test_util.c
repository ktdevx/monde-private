/* po_merge のテスト用の補助関数 */

#include "test_util.h"

#include <string.h>

#include "foundation/diag.h"
#include "foundation/file_io.h"
#include "po/po_read.h"
#include "po/po_write.h"

/* diags の各診断を note として表示し、1 つにつき 1 回、診断がないことの検査を失敗させる */
static void report_diags(const mnd_diag_list_t *diags)
{
    size_t count = mnd_diag_list_count(diags);
    size_t i;

    for (i = 0; i < count; i++)
    {
        const mnd_diag_t *diag = mnd_diag_list_at(diags, i);
        const char *path = (diag->path != NULL) ? diag->path : "(unknown)";
        const char *severity = (diag->severity == MND_SEVERITY_ERROR) ? "error" : "warning";

        if (diag->line > 0)
        {
            NAOS_NOTE("%s:%zu: %s: %s", path, diag->line, severity, diag->message);
        }
        else
        {
            NAOS_NOTE("%s: %s: %s", path, severity, diag->message);
        }

        NAOS_CHECK_UINT_EQ(count, 0);
    }

    if (diags->dropped)
    {
        NAOS_NOTE("some diagnostics were dropped (out of memory)");
        NAOS_CHECK_FALSE(diags->dropped);
    }
}

void test_read_po(const char *text, mnd_po_catalog_t *catalog)
{
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok;

    mnd_po_catalog_init(catalog);
    ok = mnd_po_read(text, strlen(text), "test.po", catalog, &diags);
    report_diags(&diags);
    mnd_diag_list_free(&diags);
    NAOS_REQUIRE_TRUE(ok);
}

void test_check_written(const mnd_po_catalog_t *catalog, const char *expected)
{
    mnd_buf_t out = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool ok;

    ok = mnd_po_write(catalog, NULL, &out, &diags);
    report_diags(&diags);
    mnd_diag_list_free(&diags);
    if (!ok)
    {
        mnd_buf_free(&out);
        NAOS_FAIL("mnd_po_write failed");
    }

    /*
     * 書き出した内容は NUL を含まないため、STR_EQ で両方の内容と最初に異なる位置を表示する。
     * 長さも比べ、バイト単位で一致することを確かめる
     */
    NAOS_CHECK_STR_EQ(mnd_buf_cstr(&out), expected);
    NAOS_CHECK_UINT_EQ(out.len, strlen(expected));
    mnd_buf_free(&out);
}

void test_read_data_file(const char *name, mnd_buf_t *content)
{
    mnd_buf_t path = MND_BUF_INIT;
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    bool had_bom = false;
    bool ok;

    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&path, MND_MERGE_TEST_DATA_DIR "/"));
    NAOS_REQUIRE_TRUE(mnd_buf_append_str(&path, name));
    ok = mnd_file_read(mnd_buf_cstr(&path), content, &had_bom, &diags);
    report_diags(&diags);
    mnd_diag_list_free(&diags);
    mnd_buf_free(&path);
    NAOS_REQUIRE_TRUE(ok);
}
