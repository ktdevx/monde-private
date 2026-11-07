/* スイート splice: 書き戻し（Splicer）（2.7、4.8〜4.10、5.2、5.4） */

#include <naos/naos.h>

#include "foundation/diag.h"
#include "tu/tu.h"
#include "tu/tu_internal.h"

#include <stddef.h>

/* 走査した文書は原文を複製せずに指し、内部の構造体からセグメントの配列を参照できる */
NAOS_TEST(splice_doc_points_to_source)
{
    static const char data[] = "";
    mnd_diag_list_t diags = MND_DIAG_LIST_INIT;
    mnd_tu_doc_t *doc = NULL;

    NAOS_REQUIRE_TRUE(mnd_tu_scan(data, 0, NULL, &doc, &diags));
    NAOS_CHECK_TRUE(doc->data == data);
    NAOS_CHECK_UINT_EQ(0, doc->len);
    NAOS_CHECK_UINT_EQ(0, doc->seg_count);
    mnd_tu_doc_free(doc);
    mnd_diag_list_free(&diags);
}

NAOS_SUITE(splice,
    NAOS_ADD_TEST(splice_doc_points_to_source)
)
